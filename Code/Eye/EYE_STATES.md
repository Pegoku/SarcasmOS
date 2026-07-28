# Eye state contract

This document defines how the two 240x240 round GC9A01A eye displays
participate in the SarcasmOS face. It is derived from:

- `../AI/Workflow/sysprompt.txt`
- `../AI/Workflow/SarcasmOS-web/backend/sysprompt.txt`
- the `render_face()` state tables in `../AI/Workflow/final1.py` and
  `../AI/Workflow/SarcasmOS-web/backend/bender_core.py`

The `render_face()` state names are canonical. Its ASCII eye strings describe
pose and emotion; they are not literal display graphics.

## Hardware baseline

There are two independent RP2040 eye boards:

- left eye: role `0`, I2C address `0x30`;
- right eye: role `1`, I2C address `0x31`;
- each board drives one 240x240 round RGB565 GC9A01A display.

Normal rendering should keep both eyes synchronized while allowing mirrored
geometry and per-eye pupils.

## Canonical Workflow states

| State | Workflow hint | Target eye behavior |
| --- | --- | --- |
| `idle` | `o o` | Neutral open eyes with subtle life motion and occasional blink |
| `thinking` | `@ @` | Focused or slowly orbiting pupils |
| `tool` | `> <` | Concentrated inward squint |
| `speaking` | `^ ^` | Confident half-squint; audio amplitude may add subtle motion |
| `left` | `<o <o` | Both pupils look left |
| `right` | `o> o>` | Both pupils look right |
| `up` | `^ ^` | Both pupils look up |
| `down` | `v v` | Both pupils look down |
| `center` | `o o` | Both pupils return to center |
| `neutral` | `o o` | Neutral open eyes |
| `sarcastic` | `- o` | One eyelid lowered asymmetrically |
| `angry` | `> <` | Inward-sloped upper lids and focused pupils |
| `happy_fake` | `^ ^` | Exaggerated cheerful squint |
| `suspicious` | `- -` | Narrow eyes with a small sideways pupil offset |
| `tired` | `u u` | Heavy lowered lids and slow movement |
| `asleep` | `- -` | Fully closed lids; displays may dim after the close animation |
| `surprised` | `O O` | Wide-open eyes with smaller centered pupils |
| `bored` | `- -` | Half-closed lids and infrequent movement |
| `dramatic` | `* *` | Wide, sparkling or star-highlighted eyes |
| `watch` | `0 0` | Alert centered eyes, tracking enabled when a target exists |
| `party` | `^ *` | Asymmetric wink/star animation |
| `error` | `X X` | Clear X/error eyes, without rapid flashing |
| `battery_low` | `_ _` | Very low lids and reduced brightness |
| `sunny` | `☀` | Full-screen animated sun; no eye is visible |
| `rainy` | `🌧` | Full-screen cloud and falling rain; no eye is visible |
| `cloudy` | `☁` | Full-screen drifting cloud; no eye is visible |
| `stormy` | `🌩` | Full-screen cloud and lightning; no eye is visible |
| `snowy` | `🌨` | Full-screen cloud and falling snow; no eye is visible |

## Direct hardware intents

The system prompt additionally defines these eye-specific intents:

| Intent | Required behavior |
| --- | --- |
| `eye.look.left` | Set gaze left without changing the persistent expression |
| `eye.look.right` | Set gaze right without changing the persistent expression |
| `eye.look.center` | Center gaze without changing the persistent expression |
| `eye.look.at_target` | Follow the supplied target coordinates, clamped to a safe eye range |
| `eye.blink` | Play one blink, then restore the prior gaze and expression |
| `eye.close` | Close both eyes until another command opens or changes them |
| `eye.expression.angry` | Set persistent expression to `angry` |
| `eye.expression.happy` | Set persistent expression to `happy_fake` |
| `eye.expression.suspicious` | Set persistent expression to `suspicious` |
| `eye.expression.sleep` | Enter `asleep` |

Tracking coordinates should be smoothed and applied equally to both eyes. A
blink must not reset gaze or expression.

## State composition and precedence

Eye control needs independent persistent expression, transient activity, and
gaze values. The eyes apply this precedence:

1. `asleep` or explicit `eye.close`;
2. safety/status overlays such as `error` and `battery_low`;
3. a one-shot blink;
4. transient activity pose;
5. persistent expression;
6. gaze or target tracking layered into the selected pose.

When an activity or blink ends, the previous expression and gaze are restored.
Mouth speaking animation does not force the pupils back to center.

## Firmware status

The RP2040 firmware supports all animation IDs `0x00` through `0x1e` using
paired, editable 240x240 left/right artwork. Every state has a distinct eye
pose or animation, including directional gaze, Bender-style angry and laugh
expressions, activity states, status states, and weather accents. Unknown
future expressions render as `neutral`.

The current byte-sized animation command still selects one state at a time.
Independent persistent expression, transient activity, gaze composition, and
continuous target coordinates remain future protocol work.
