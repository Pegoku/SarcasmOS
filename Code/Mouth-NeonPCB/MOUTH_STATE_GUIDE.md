# SarcasmOS mouth state guide

This guide explains every visual state supported by the 64x32 mouth display.
It is written for users, artists, and integrators who do not already know the
firmware or protocol.

## What a mouth state is

A state is the visual mode currently selected on the mouth. Examples include
`idle`, `thinking`, `speaking`, and `angry`. Each state has:

- a numeric protocol ID;
- a semantic meaning for the user;
- one or more native 64x32 artwork frames;
- a frame speed and playback rule;
- an event that should select it and another event that should replace it.

The mouth is a passive renderer. It does not listen to the microphone, inspect
the weather, read the battery, or decide that Bender is angry. The Brain or
another controller must send the corresponding state over ESP-NOW.

Receiving `SET_ANIMATION` (`0x20`) or `SET_EXPRESSION` (`0x21`) selects a
state. Receiving `STOP` (`0x23`) selects `asleep`. The display keeps the last
valid state until another command changes it. There is no automatic state
timeout in the mouth firmware.

## Two different meanings of “duration”

It is useful to separate these two timings:

1. **State lifetime** is how long the meaning remains active. This is
   event-driven. For example, `speaking` should normally last until playback
   ends. At firmware level it lasts indefinitely if no replacement arrives.
2. **Animation cycle** is how long the artwork takes to repeat while that
   state remains active. For example, `thinking` repeats every 864 ms.

Static states have one frame and therefore no visible animation cycle. Their
stored 1000 ms frame time is only a harmless asset-format value.

## State families

The 31 supported states fall into four practical families:

- **Activity states** explain what the assistant is doing: listening,
  thinking, using a tool, or speaking.
- **Expression states** convey attitude or emotion: neutral, sarcastic,
  angry, suspicious, and similar.
- **Gaze states** describe eye direction. Their mouth artwork deliberately
  stays neutral because looking left should not change the mouth.
- **Status/context states** report sleep, errors, battery condition, weather,
  or special modes.

`idle` and `neutral` look similar but mean different things. `idle` means the
assistant is ready and not doing work. `neutral` is a persistent facial
expression that can be restored after a temporary activity.

## Complete state reference

### Activity and conversation states

| ID | State | What it tells the user | Typical cause | Normal exit/lifetime | Mouth appearance and motion |
| ---: | --- | --- | --- | --- | --- |
| `0x00` | `idle` | The assistant is awake, ready, and not processing a request. | Boot has completed, a request has finished, or idle was explicitly requested. | Until recording, processing, an expression, an alert, or sleep begins. Firmware timeout: none. | Static pale-yellow Bender tooth grille. |
| `0x01` | `listening` | The robot is actively accepting input from the user. | Microphone capture or recording starts. | Until capture stops; normally followed by a thinking state. Firmware timeout: none. | A subtle scan moves across the resting grille; 8 frames, 192 ms each, 1.536 s cycle. |
| `0x02` | `thinking` | The request was received and the assistant is transcribing, planning, or generating an answer. It is not stuck merely because this animation repeats. | Speech transcription, LLM generation, tool planning, or Brain booting. | Until a tool starts, an answer is ready, speaking begins, or an error occurs. Firmware timeout: none. | Three paced thinking markers across the grille; 3 frames, 288 ms each, 864 ms cycle. |
| `0x03` | `thinking_audio` | Audio is specifically being uploaded, decoded, or transcribed. It is a faster version of thinking. | The web Workflow enters loading mode for an audio request. | Until audio processing completes or changes to ordinary thinking, speaking, or error. Firmware timeout: none. | Faster three-step marker motion; 3 frames, 192 ms each, 576 ms cycle. |
| `0x04` | `thinking_long` | Processing has taken longer than usual, but is still in progress. This is feedback, not itself an error. | In the web Workflow, thinking has continued for 3.8 seconds. | Until processing completes or fails. Firmware timeout: none. | Deliberately slower three-step thinking motion; 3 frames, 448 ms each, 1.344 s cycle. |
| `0x05` | `speaking` | The assistant is producing audible speech. | TTS playback starts or `mouth.talk` is requested. | Ideally while audio is audible, then restore the previous expression or `idle`. Firmware timeout: none. | A solid teeth-colored waveform replaces the internal tooth lines from roughly one-quarter to three-quarters of the screen. Eight amplitude frames are evaluated every 32 ms; the nominal up/down sequence is 448 ms. |
| `0x0a` | `tool` | The assistant is calling an external function such as weather, time, calendar, or device status. | A model tool/function call begins. | Until the tool returns; then resume thinking, show its resulting expression, speak, or show error. Firmware timeout: none. | Firm bar with a moving progress marker; 8 frames, 192 ms each, 1.536 s cycle. |

### Expression states

These states normally persist until explicitly changed or temporarily covered
by an activity/status state.

| ID | State | What it tells the user | Typical cause | Mouth appearance and motion |
| ---: | --- | --- | --- | --- |
| `0x06` | `happy_fake` | A deliberately artificial Bender-style happy face rather than a claim of genuine emotion. `happy` is an alias for this state. | An explicit happy-expression request or `set_expression("happy_fake")`. | Static small teeth-colored diamond in the current artwork. |
| `0x07` | `angry` | Anger, frustration, determination, or theatrical hostility. It should not imply a safety fault. | An explicit expression request or dialogue policy. | Static tight, clenched tooth grille. |
| `0x10` | `neutral` | No special emotion is being expressed. This is the safe expression fallback. | Explicit neutral request, expression reset, or restoration after a transient activity. | Static resting tooth grille. |
| `0x11` | `sarcastic` | Sarcasm, smugness, or a Bender-like smirk. | `mouth.smirk`, an explicit sarcastic request, or dialogue policy. | Static asymmetric raised smirk. |
| `0x12` | `suspicious` | Skepticism, doubt, or careful examination. | Explicit suspicious expression or a context-sensitive dialogue decision. | Static small skeptical dip/off-center mouth. |
| `0x13` | `tired` | Low energy or fatigue. This is an expression unless a separate power policy selects it. | Explicit tired expression or a future fatigue/inactivity policy. | Dim, shallow two-frame mouth; 640 ms per frame, 1.280 s cycle. |
| `0x14` | `surprised` | Surprise, shock, or sudden discovery. | Explicit surprised expression or a notable event. | Static small rounded open mouth. |
| `0x15` | `bored` | Disinterest or prolonged inactivity. | Explicit bored expression or a future idle-personality timer. | Static flat, minimally energetic mouth. |
| `0x16` | `dramatic` | Deliberate exaggeration for a theatrical response. | Explicit dramatic expression or dialogue policy. | Three sizes pulse forward and backward; 192 ms per step, 768 ms full ping-pong cycle. |
| `0x18` | `party` | Celebration, success, or party mode. | Explicit party request, event, or achievement. | Wide animated smile using accent colors; 6 frames, 144 ms each, 864 ms cycle. |

### Gaze and attention states

The names below mainly describe the eyes. The mouth uses a static neutral
grille in all five states. They exist as shared whole-face IDs so older or
simple controllers can send one synchronized state to all displays.

| ID | State | Typical cause | How long it should last |
| ---: | --- | --- | --- |
| `0x0b` | `left` | `eye.look.left` or tracking a target to the left. | Until gaze changes or tracking ends. |
| `0x0c` | `right` | `eye.look.right` or tracking a target to the right. | Until gaze changes or tracking ends. |
| `0x0d` | `up` | `eye.look.up` or tracking a target above. | Until gaze changes or tracking ends. |
| `0x0e` | `down` | `eye.look.down` or tracking a target below. | Until gaze changes or tracking ends. |
| `0x0f` | `center` | `eye.look.center`, loss of a tracked target, or gaze reset. | Until another gaze command. |

Changing gaze should not destroy a persistent mouth expression in a fully
composited face controller. The present one-byte protocol cannot represent
gaze and expression simultaneously, so the Brain should route gaze to the
eyes without replacing the mouth state where possible.

### Status, context, and environment states

| ID | State | What it tells the user | Typical cause | Normal exit/lifetime | Mouth appearance and motion |
| ---: | --- | --- | --- | --- | --- |
| `0x08` | `error` | A real operation or hardware/software action failed. It should not be used merely for an angry personality response. | Matrix/ESP-NOW initialization failure, failed tool call, invalid subsystem state, or explicit fault report. | Until recovery is confirmed or the user dismisses the error. Firmware timeout: none. | Slow red two-frame warning, 1.024 s per frame, 2.048 s cycle; deliberately avoids rapid flashing. |
| `0x09` | `asleep` | The face/display is sleeping or intentionally off. Protocol and Brain code may call this `sleep`. | `STOP`, explicit sleep, shutdown policy, or display-off request. | Until an explicit wake/new-animation command or reboot. | Fully black panel. |
| `0x17` | `watch` | The assistant is checking sensors, device status, or another live condition. | `robot_status`, sensor query, or monitoring action. | Until the status result is available. Firmware timeout: none. | Alert progress/scan in warm yellow; 9 frames, 192 ms each, 1.728 s cycle. |
| `0x19` | `battery_low` | Available battery energy is low and charging or shutdown may soon be needed. | Intended to come from a validated fuel-gauge threshold or explicit test; never from a guessed value. | Until the battery recovers above a hysteresis threshold, charging policy clears it, or sleep begins. Firmware timeout: none. | Dim sagging warning; 4 frames, 500 ms each, 2.000 s cycle. |
| `0x1a` | `sunny` | The most recent real weather result is sunny or clear. | Weather tool result containing clear/sunny conditions and a real temperature. | Until another expression/state replaces it; weather should be refreshed before treating it as current. | Static upturned grin with centered black temperature text. |
| `0x1b` | `rainy` | The most recent real weather result reports rain, drizzle, or showers. | Weather tool result containing rain-related conditions and a real temperature. | Until another expression/state replaces it. Firmware timeout: none. | Slow wave/downturn in rain color; 8 frames, 128 ms each, 1.024 s cycle, with centered black temperature text. |
| `0x1c` | `cloudy` | The most recent real weather result reports cloud or overcast conditions. | Weather tool result containing cloud/overcast conditions and a real temperature. | Until another expression/state replaces it. Firmware timeout: none. | Soft subdued oval; 3 frames, 480 ms each, 1.440 s cycle, with centered black temperature text. |
| `0x1d` | `stormy` | The most recent real weather result reports a storm or thunder. | Weather tool result containing thunder/storm conditions and a real temperature. | Until another expression/state replaces it. Firmware timeout: none. | Angular zigzag in storm color; 6 frames, 160 ms each, 960 ms cycle, with centered black temperature text. |
| `0x1e` | `snowy` | The most recent real weather result reports snow or sleet. | Weather tool result containing snow/sleet conditions and a real temperature. | Until another expression/state replaces it. Firmware timeout: none. | Sparse gentle snow-colored dots; 6 frames, 288 ms each, 1.728 s cycle, with centered black temperature text. |

Weather states must follow a real weather lookup. The system prompt explicitly
forbids inventing weather data, sensor values, battery readings, and completed
hardware actions.

`SET_PARAM` (`0x30`) with key `2` supplies temperature as a signed int8 number
of degrees Celsius. The usual two-character numeric forms, such as `30` and
`-5`, use equal-width character cells and therefore share the same horizontal
center. Wider valid values such as `-10` are measured and recentered rather
than clipped to two characters. The complete display text is `30°C`, `-5°C`,
and so on. The supported live-data range is `-127..127`; byte value `-128`
means “temperature unavailable” and hides the text. The value persists across
all five weather states until updated or cleared. The normal firmware starts
unavailable, while the local emulator/test firmware uses `30°C` as obvious
test data.

## Speaking intensity

`speaking` is the only state whose frame selection uses a live parameter.
`SET_PARAM` (`0x30`) with key `1` sets mouth intensity from `0` to `255`.

- Low values keep the waveform shallow.
- The default is `120`.
- Around `120`, the animation traverses its designed amplitude range.
- Higher values reach the largest frames sooner.
- The animation still repeats if the intensity is not updated, so the Brain
  should stop or replace `speaking` when audio ends.

The firmware redraws at most every 40 ms (25 frames per second), even though
the speaking asset phase advances in 32 ms steps. For live audio, the Brain
integration should send fresh intensity values at roughly 20-25 Hz and discard
stale queued values.

The current web UI uses audio RMS thresholds to decide when speech is audible:
voice starts around `0.028`, stops around `0.014`, and is held through pauses
for 180 ms. Those numbers belong to the web audio analyser; they are not
currently transmitted to the mouth as intensity automatically.

## State selection, replacement, and precedence

The desired face model contains three independent values:

1. a persistent expression, such as `sarcastic`;
2. a transient activity, such as `thinking` or `speaking`;
3. eye gaze/tracking.

A useful conceptual priority is:

1. display-off/`asleep`;
2. safety overlays such as `error` and `battery_low`;
3. active `speaking`;
4. other activity states;
5. the persistent expression.

The current mouth firmware does **not** store that stack. It stores one
animation ID, and the last valid command wins. It also does not remember which
expression to restore after speaking. The Brain must currently maintain those
values and send the correct restoration state. A future composited face
command could carry them independently.

## What is automatic today

There are three layers with different coverage:

### Mouth firmware

- Supports every ID `0x00` through `0x1e`.
- Starts in `idle`.
- Enters `error` automatically if ESP-NOW initialization fails after the
  matrix has initialized.
- Enters `asleep` when it receives `STOP`.
- Otherwise changes only when a valid command is received.
- Rejects unknown/out-of-range IDs rather than displaying arbitrary memory.

### Current Brain firmware

The Brain currently has seven high-level assistant states:
`booting`, `idle`, `listening`, `thinking`, `speaking`, `error`, and `sleep`.
It sends those over ESP-NOW, but its HTTP parser presently:

- collapses `thinking_audio` and `thinking_long` into `thinking`;
- maps `happy` to `idle`;
- maps `angry` to `error`;
- defaults unrecognized expressions to `idle`.

Therefore, all 31 animations are supported by the mouth and test tools, but
many are not yet reachable through the Brain's high-level HTTP API. Direct
protocol commands or future Brain state-machine work are needed for full
coverage. In particular, anger should eventually be kept distinct from a real
error.

### Workflow software

The CLI/backend uses:

- `thinking` during transcription, model generation, and tool planning;
- `tool` while a function call executes;
- `watch` while checking robot status;
- `error` when a tool call throws an exception;
- `speaking` while synthesizing/downloading/playing an answer;
- weather expressions after a real weather tool result;
- explicit expression states when a supported expression is requested.

The web front end adds `thinking_audio` for audio requests and marks thinking
as long after 3.8 seconds. These visual decisions currently affect the
software/web face; a bridge still needs to forward each transition to the
Brain for the physical mouth to mirror it.

## Timing and synchronization details

- The panel is redrawn every 40 ms, so a newly accepted state normally becomes
  visible on the next render update.
- ESP-NOW transport and Brain scheduling add their own latency. The integration
  target is under 100 ms from state decision to visible animation.
- Animation phase is based on the ESP32 millisecond clock plus an optional
  synchronization phase. Selecting a state does not reset its phase to frame
  zero, so synchronized displays can enter a repeating animation together.
- `SYNC` (`0x22`) changes phase; it does not change the selected state.
- Brightness changes do not change state or animation timing.
- The mouth's power-on brightness is `64/255`; the current Brain normally
  overrides it with `160/255` during startup.

## Typical conversation sequence

A complete voice request should look approximately like this:

```text
idle
  -> listening        microphone capture starts
  -> thinking_audio   audio upload/transcription
  -> thinking         model plans the answer
  -> tool             optional external tool call
  -> thinking         model uses the tool result
  -> speaking         audible response playback
  -> idle or saved persistent expression
```

If processing passes 3.8 seconds, `thinking_long` can replace the relevant
thinking state. Any failed stage may select `error`. A sleep request selects
`asleep` and remains there until a wake/new-state command.

## Names and aliases

| Name seen elsewhere | State used by the asset pack | Notes |
| --- | --- | --- |
| `happy` | `happy_fake` (`0x06`) | Protocol constant is currently named `kAnimHappy`. |
| `sleep` | `asleep` (`0x09`) | Protocol constant is currently named `kAnimSleep`. |
| `thinking-audio` | `thinking_audio` (`0x03`) | Hyphenated CSS name versus underscore protocol/asset name. |
| `thinking-long` | `thinking_long` (`0x04`) | Hyphenated CSS name versus underscore protocol/asset name. |
| `mouth.talk` | `speaking` (`0x05`) | Should be driven by audio intensity. |
| `mouth.smirk` | `sarcastic` (`0x11`) | Persistent expression intent. |

## For artists and testers

Run the desktop emulator from this directory:

```sh
./emulator.py
```

- Left/Right changes state.
- Space pauses both state cycling and animation frames.
- Up/Down selects a frame while paused.
- `E` opens the current native 64x32 frame in GIMP.
- `O` opens every frame of the current animation in GIMP.
- Overwriting an opened PPM automatically updates the shared asset pack and
  firmware header.
- In an animation edit folder, adding or deleting a one-based numbered PPM
  adds or removes that frame. Gaps are compacted automatically.

The authoritative artwork and timing live in
`assets/mouth_assets.json`. Both emulator and firmware use that file. Do not
edit `generated/mouth_assets.hpp` manually.

## Source files

- `assets/mouth_assets.json`: artwork, palette, frame order, and frame timing.
- `protocol.hpp`: mouth-side IDs and commands.
- `../Brain/components/mouth_espnow/include/display_protocol.h`: matching
  Brain-side protocol.
- `../Brain/main/main.c`: current high-level Brain state mapping.
- `../AI/Workflow/final1.py` and
  `../AI/Workflow/SarcasmOS-web/backend/bender_core.py`: Workflow state intent.
- `../AI/Workflow/SarcasmOS-web/app.js`: web thinking/audio timing.
- `MOUTH_STATES.md`: concise visual contract and intended composition model.
