# Brain eye/mouth transition coordination implementation

## Scope

This document describes the changes required in `Code/Brain`. It is an
implementation plan only; it does not modify the Brain firmware.

The Brain must become the coordinator for face state changes. It should ask
both eyes to finish their current animation, wait until both have activated
the requested target, and only then command Mouth-NeonPCB to blend to that
target.

See also:

- [`MOUTH_TRANSITION_IMPLEMENTATION.md`](MOUTH_TRANSITION_IMPLEMENTATION.md)
- [`EYE_TRANSITION_IMPLEMENTATION.md`](EYE_TRANSITION_IMPLEMENTATION.md)

## Current problem

`set_animation_all()` currently calls `display_command_all()`, which sends
`SET_ANIMATION` to both I2C eyes and the ESP-NOW mouth immediately. Eye queues
the target and exits gracefully, while Mouth-NeonPCB switches immediately.
This leaves the mouth in the new state while the eyes still display the old
one.

The Brain also only transmits to the eyes. It does not read and validate their
status responses, so it cannot know when `activate_animation()` ran.

## Required state-change sequence

Replace the broadcast-style state change with this barrier:

```text
Workflow requests target state
        |
Brain allocates transition token
        |
send target + token to left and right eyes only
        |
poll both eye status responses
        |
both report target active with matching token?
        | yes
send target + token + duration to ESP-NOW mouth
        |
mouth acknowledges and begins 200 ms blend
```

The old mouth animation remains active during the eye exit. This is required,
not a timeout artifact.

## Split transport helpers

Remove the mouth send from `display_command_all()`. Replace it with helpers
whose names make their scope explicit:

```c
esp_err_t eye_command(display_device_t *eye, ...);
esp_err_t eye_command_both(...);
esp_err_t mouth_command(...);
```

Brightness and reset commands may still call both transport-specific helpers
back-to-back because they do not participate in the animation barrier.

Animation commands must go through a dedicated face-transition coordinator;
no HTTP handler or state task should send `CMD_SET_ANIMATION` directly.

## Read and validate Eye status

Add an I2C read helper using `i2c_master_receive()` after the Eye response
format in `EYE_TRANSITION_IMPLEMENTATION.md` is implemented. Read the exact
response length and validate:

- protocol version;
- device role matches the expected left/right device;
- firmware version is supported;
- animation ID is valid;
- status belongs to the requested transition token;
- last error is zero.

Extend `display_device_t` with:

```c
uint8_t active_animation;
uint8_t pending_animation;
uint8_t active_transition_token;
uint8_t pending_transition_token;
uint8_t playback_flags;
uint8_t current_frame;
uint32_t status_read_ms;
```

An I2C transmit success only establishes physical presence. It must not be
treated as animation completion.

## Coordinator task and queue

Do not block the HTTP server task or the existing state poll loop while an eye
animation finishes. Add:

- a one-element FreeRTOS queue containing the newest requested face state;
- a dedicated `face_transition_task`;
- a mutex protecting the desired state and transition token.

A one-element overwrite queue is intentional. If the Workflow requests
`thinking`, then `speaking` before the eyes finish leaving `listening`, the
coordinator should discard the obsolete `thinking` target and transition
directly to `speaking`.

Suggested coordinator flow:

1. Receive the newest desired animation.
2. Increment the token, skipping zero.
3. Send `{animation, token, durationTicks}` to both eyes.
4. Poll each eye every 20 ms.
5. Mark an eye ready only when its active animation and active token match,
   it has no pending animation, and it reports no error.
6. If a newer desired state arrives, restart at step 2 with a new token.
7. When both eyes are ready, send the same payload to the mouth and wait for
   its ESP-NOW acknowledgement.
8. Record the committed face state and transition token.

Do not send `CMD_SYNC` until after the barrier. The current Eye firmware
ignores its absolute uptime payload. If animation phase synchronization is
still desired, define it separately as elapsed phase and send it after all
three displays have activated the target.

## Timeouts and degraded operation

Use named, configurable limits:

- eye status poll interval: 20 ms;
- normal eye barrier timeout: 2500 ms;
- mouth acknowledgement: retain the existing ESP-NOW retry policy;
- visual transition duration: 200 ms.

On eye timeout:

1. Log which eye failed and its last complete status.
2. Mark that eye unavailable.
3. If at least one eye completed, allow the mouth transition after the
   2500 ms limit so one failed board cannot freeze the entire face forever.
4. If neither eye is reachable, allow a degraded immediate mouth transition
   after a shorter presence check and report the face as degraded.
5. Never treat a stale matching animation ID with the wrong token as success.

Error and safety states may bypass the graceful barrier. Define an explicit
priority flag for `error`, shutdown, and battery-critical states:

- send the target to both eyes;
- command the mouth immediately with a short transition;
- do not wait for eye completion.

This bypass must be deliberate and limited to those states. Ordinary rapid
Workflow changes should use queue replacement, not the emergency path.

## Duplicate dispatch cleanup

The current HTTP handler calls `set_animation_all()` directly after updating
`g_state`, and `animation_task()` can send the same state again. Replace both
paths with one `request_face_state()` function that only updates the desired
state/queue.

Likewise, startup should enqueue `ANIM_IDLE` once instead of sending it
through multiple paths.

## Protocol ownership

The animation IDs and extended fields are currently duplicated across:

- Brain `components/mouth_espnow/include/display_protocol.h`;
- Mouth-NeonPCB `protocol.hpp`;
- Eye `include/protocol.hpp`.

Keep their values identical and bump the relevant application/I2C versions
together. Longer term, generate these headers from one machine-readable
protocol definition, but that refactor is not required for the first
transition implementation.

## Status API

Extend `/api/status` with:

```json
{
  "face_transition": {
    "desired_animation": 5,
    "committed_animation": 1,
    "token": 42,
    "waiting_for": ["right_eye"],
    "started_ms": 123456,
    "degraded": false
  }
}
```

The command endpoint may continue returning immediately, but it should say
that the request was queued. If callers require completion, add an optional
blocking/status endpoint keyed by the transition token rather than holding
the original HTTP request indefinitely.

## Verification

Add tests or a hardware-in-the-loop test mode for:

1. Brain does not send the mouth target before both eyes report the matching
   active token.
2. An eye's command acknowledgement does not count as activation.
3. Left and right eyes can finish at different times.
4. A rapid second state request replaces the first pending target.
5. Stale status with the correct animation but wrong token is rejected.
6. One missing eye triggers the documented timeout and degraded behavior.
7. ESP-NOW mouth failure is reported after the eye barrier completes.
8. Emergency error state bypasses the normal wait.
9. Startup sends one coordinated idle request.
10. HTTP and Workflow state changes use the same coordinator path.

Run the Brain component tests and a three-board bench test. During the bench
test, log timestamps for:

- eye request sent;
- left eye activated;
- right eye activated;
- mouth command sent;
- mouth transition started and completed.

The mouth command timestamp must never precede either eye activation timestamp
for a normal transition. Do not flash any board automatically.
