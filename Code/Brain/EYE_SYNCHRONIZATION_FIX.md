# Eye synchronization implementation specification

## Problem and root cause

The Brain already sends the same animation and transition token to the left and
right Eye in immediate succession. At 400 kHz I2C, the transmission skew is
small and is not the source of a visibly long offset.

The Eye firmware does not use the token as a start barrier. Each Eye currently:

1. receives `SET_ANIMATION` independently;
2. asks its locally running animation to exit;
3. finishes that exit from its own current frame and direction; and
4. immediately activates the pending animation on its own local clock.

This happens in `src/main.cpp`: `request_animation()` calls
`eye_playback::request_exit()`, and `update_animation_playback()` calls
`activate_animation()` as soon as that individual Eye finishes. The two Eyes
can be at different outgoing frames, and one main loop can be occupied sending
a 240x240 frame to its display. Consequently, one Eye may activate well before
the other. `kCmdSync` currently does nothing. The 13-byte status and transition
token let Brain observe the difference after it occurs; they do not prevent it.

Do not try to fix this by adding an arbitrary delay between Brain's two
`SET_ANIMATION` writes. That only changes which Eye is late. Likewise, polling
faster cannot synchronize activation that has already happened.

## Required behavior

- Both Eyes may finish their outgoing animations independently.
- An Eye that finishes first must hold its legal exit frame and report READY;
  it must not activate the destination yet.
- Brain waits until every requested Eye is READY.
- Brain then commits both Eyes with a short future start delay.
- Both Eyes activate the destination/token at that deadline and start at frame
  zero. Expected visible skew should be at most one I2C transaction plus local
  scheduling jitter, preferably below 1 ms and never more than one frame.
- A missing Eye must not leave the other Eye frozen forever.

## Eye firmware changes

### 1. Add an armed/ready transition phase

Extend `eye_transition::State` in `include/animation_transition.hpp` with a
ready/armed state. A queued destination still has `pending_animation` and
`pending_token`, but reaching the legal exit boundary must set `ready = true`
instead of calling `activate_pending()`.

In `update_animation_playback()`:

- when `eye_playback::advance()` says the outgoing animation is complete and a
  pending destination exists, set READY and hold the final legal exit frame;
- do not advance the outgoing playback again while READY;
- do not call `activate_animation()` until a matching commit is armed and its
  deadline has arrived.

A new request with a newer token replaces the pending destination and clears
READY/commit state. Repeating the identical animation/token remains idempotent.
A request for the already-active animation with a new token must be defined
explicitly: preferably treat it as an immediately READY restart so Brain can
resynchronize two Eyes that have drifted while showing the same state.

### 2. Report READY in the existing status

Use bit 3 of status byte 11 (`playback_flags`) as
`EYE_PROTOCOL_FLAG_READY`. Preserve the existing meanings:

- bit 0: pending;
- bit 1: exiting;
- bit 2: activated/no pending;
- bit 3: ready at the outgoing exit barrier.

While READY, status must continue reporting the outgoing animation/token as
active and the requested destination/token as pending. Update the status
snapshot immediately when READY changes.

### 3. Implement `CMD_SYNC` as the commit operation

Use a versioned payload so malformed or stale commits are rejected:

```text
CMD_SYNC payload (4 bytes)
byte 0: transition token
byte 1: start delay, little-endian milliseconds low byte
byte 2: start delay, little-endian milliseconds high byte
byte 3: flags; must currently be zero
```

Accept the command only when:

- a pending destination exists;
- the Eye is READY;
- the supplied token equals `pending_token`; and
- the delay is within a bounded range, for example 10-250 ms.

Record the I2C STOP reception timestamp in the IRQ handler and calculate the
deadline from that timestamp, not from the later `parse_command()` call. This
prevents a display transfer from adding different parsing delays on the two
boards. A stale/wrong token must leave the current transition untouched and set
an error visible in status.

Start with a 50 ms delay. That is long enough for both RP2040 main loops to
finish parsing the sequential commit packets before either deadline.

### 4. Make the visible first frame honor the deadline

At the commit deadline:

- atomically make the pending animation/token active;
- clear pending, READY, and commit state;
- reset playback to frame zero;
- set both `frame_started_ms` and `animation_started_ms` to the scheduled
  deadline, not the time at which the main loop happened to notice it;
- render frame zero immediately.

Avoid beginning a full display transfer when an armed deadline is closer than
the measured worst-case `draw_eye()` duration. Otherwise one Eye can be trapped
inside an SPI transfer while the other begins. If a deadline is nevertheless
noticed late, derive playback position from elapsed time since the scheduled
deadline so both Eyes converge to the same frame rather than permanently
retaining the offset.

Do not add an arbitrary remote uptime directly to the frame index. The only
time reference here is the locally calculated commit deadline.

### 5. Timeout and emergency behavior

- If READY is reached but no valid commit arrives, hold for a bounded timeout
  and then either cancel safely or remain on the outgoing exit pose while
  reporting an error. Do not silently activate early.
- Provide a force/immediate flag only if emergency states need to skip graceful
  exit. Even emergency changes should be armed on both Eyes first and committed
  with a small future delay.
- Watchdog servicing must continue while READY and while waiting for a commit.

## Brain changes required after Eye support exists

Production `main/main.c` and `self_test/main/main.c` must then use a two-phase
barrier:

1. send the same `SET_ANIMATION {animation, token, mouth_duration}` to both
   selected Eyes;
2. poll the direct 13-byte status until each sent Eye reports pending
   animation/token plus `EYE_PROTOCOL_FLAG_READY`;
3. send `CMD_SYNC {token, 50 ms, flags=0}` to both Eyes back-to-back while
   holding the I2C mutex for the pair;
4. poll until both report the destination/token active with no pending state;
5. release the mouth transition at the intended coordinated point.

If one Eye fails or times out, commit the READY Eye rather than leaving it
stuck, mark the transition degraded, and log the missing role. The self-test
face controller must use the same algorithm so it tests production behavior.

Until the Eye firmware implements READY plus commit, Brain cannot guarantee a
simultaneous start without discarding the graceful-exit behavior.

## Verification

Add host tests for at least these cases:

- Eyes begin on different frames/directions and reach READY at different times;
- neither activates before a matching commit;
- wrong-token and duplicate commits are harmless;
- both activate frame zero at the scheduled deadline;
- a late main-loop wake-up catches up to the correct common frame;
- replacement requests clear the old READY/commit state;
- same-animation/new-token requests can resynchronize drift;
- one absent Eye times out without freezing the present Eye.

On hardware, expose the activation event on the RP2040 LED or a spare test GPIO
and capture both boards with a logic analyzer. Measure I2C commit STOP-to-GPIO
latency and left/right GPIO skew. Also record Brain's send/READY/commit logs and
confirm the visible frame indices remain equal after activation.
