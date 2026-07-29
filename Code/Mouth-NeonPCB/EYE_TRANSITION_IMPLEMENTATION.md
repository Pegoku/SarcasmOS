# Eye animation-completion reporting implementation

## Scope

This document describes the changes required in `Code/Eye`. It is an
implementation plan only; it does not modify the Eye firmware.

The Eye firmware already has the correct local behavior:

- `request_animation()` stores `pending_animation`;
- `eye_playback::request_exit()` asks the active animation to leave its loop;
- `eye_playback::advance()` follows the remaining exit path;
- `activate_animation()` switches only when that path is complete.

Do not replace this with an immediate switch. The missing feature is an
unambiguous status signal that tells the Brain when each eye has actually
activated a particular requested state.

See also:

- [`MOUTH_TRANSITION_IMPLEMENTATION.md`](MOUTH_TRANSITION_IMPLEMENTATION.md)
- [`BRAIN_TRANSITION_IMPLEMENTATION.md`](BRAIN_TRANSITION_IMPLEMENTATION.md)

## Transition token

Extend the Eye `SET_ANIMATION` and `SET_EXPRESSION` payload:

| Offset | Meaning |
| ---: | --- |
| 0 | Destination animation ID |
| 1 | Transition token assigned by the Brain |
| 2 | Mouth transition duration in 40 ms ticks; ignored by Eye |

The Eye must store the token beside `pending_animation`:

```cpp
static uint8_t pending_transition_token;
static uint8_t active_transition_token;
```

When `request_animation(animation, token)` is called:

1. Validate the animation.
2. If the same target and token are already pending, do nothing.
3. If the target is already active and no animation is pending, mark the token
   active immediately and refresh status.
4. Otherwise store both pending values and request a graceful exit.

When `activate_animation()` finally runs, copy
`pending_transition_token` to `active_transition_token` before clearing the
pending state. This moment is the synchronization barrier the Brain needs.

Tokens may wrap from `255` to `1`; reserve `0` for legacy commands. Equality,
not numerical ordering, determines whether a status matches a request.

## Extended status response

The current eight-byte I2C response only includes the active animation. Extend
it to:

| Offset | Meaning |
| ---: | --- |
| 0 | Protocol version |
| 1 | Device role |
| 2 | Firmware major |
| 3 | Firmware minor |
| 4 | Active animation |
| 5 | Last accepted command sequence |
| 6 | Last error |
| 7 | Brightness |
| 8 | Pending animation, or `0xFF` when none |
| 9 | Active transition token |
| 10 | Pending transition token, or `0` when none |
| 11 | Playback flags |
| 12 | Current frame |

Playback flags:

- bit 0: an animation is pending;
- bit 1: the active animation is following its exit path;
- bit 2: the pending target has activated for the token in byte 9.

Bump the Eye I2C protocol version because the response length and semantics
change. The Brain must reject unknown versions rather than treating truncated
or stale data as completion.

`prepare_status_response()` must be called whenever any of these values
changes:

- a command is accepted;
- graceful exit starts;
- a frame advances while exiting;
- a pending animation activates;
- an error is recorded.

The I2C interrupt currently streams `tx_buf` directly. Continue to prepare the
response outside the interrupt and copy all fields before publishing the new
`tx_len`, so the Brain cannot read a response containing fields from two
different states.

## Brain completion rule

The Eye is considered ready only when one complete status response satisfies
all of these:

```text
active_animation == requested_animation
active_transition_token == requested_token
pending_animation == 0xFF
pending flag == 0
last_error == 0
```

The accepted command sequence alone is insufficient: it only proves that the
request was queued, not that the current animation finished.

Both eyes must independently satisfy the rule. The Brain must not infer that
the right eye completed because the left eye did.

## Handling replacement requests

If a different request arrives while an exit is already in progress, replace
the pending animation and pending token, but continue the current exit path.
This collapses rapid state changes to the newest desired state and avoids
playing obsolete intermediate states.

If the desired state returns to the current animation before the exit
completes, cancel the pending request and clear `playback_state.exiting` only
if doing so leaves the playback state valid. Add a unit test for this case;
otherwise allow the current exit to complete and reactivate the same state
from frame zero.

## Tests

Extend `tests/animation_playback_test.cpp` or add a protocol/status test that
covers:

1. A loop animation reports pending until its legal exit point.
2. A ping-pong animation returns through its exit path before activation.
3. The active token changes only in `activate_animation()`.
4. Duplicate animation/token requests do not restart playback.
5. A newer pending request replaces an older one.
6. Status reports active and pending animation IDs correctly.
7. Token wraparound works.
8. Both left and right role builds produce the same completion semantics.
9. A single-frame current animation activates the pending target after the
   current frame has been shown for its configured interval.

Build both roles and run the host playback tests. Do not flash either eye
automatically.
