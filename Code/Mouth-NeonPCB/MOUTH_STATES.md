# Mouth state contract

This document defines how the 64x32 HUB75 mouth participates in the
SarcasmOS face. It is derived from:

- `../AI/Workflow/sysprompt.txt`
- `../AI/Workflow/SarcasmOS-web/backend/sysprompt.txt`
- the `render_face()` state tables in `../AI/Workflow/final1.py` and
  `../AI/Workflow/SarcasmOS-web/backend/bender_core.py`

The `render_face()` state names are the canonical names. The ASCII mouth
strings in that code express intent, not literal pixel art.

## Visual baseline

The resting mouth is Bender's pale-yellow rounded grille:

- five tooth columns and three tooth rows;
- a dark outline and dark separators;
- logical column zero at the physical left edge;
- low enough brightness for continuous use.

Speaking replaces the internal horizontal separators with a waveform running
approximately from one quarter to three quarters of the panel width. Speaking
intensity controls the waveform amplitude. It does not move the entire grille
or change the panel's logical origin.

## Canonical Workflow states

The target column describes the intended visual language. Only **idle**,
**speaking**, and **asleep** currently have distinct mouth rendering; the
current firmware holds the resting grille for the other states.

| State | Workflow hint | Target mouth behavior |
| --- | --- | --- |
| `idle` | `-----` | Resting tooth grille |
| `thinking` | `#####` | Resting grille; thinking motion belongs primarily to the eyes |
| `tool` | `=====` | Firm resting grille while a tool runs |
| `speaking` | `#####` | Centered, intensity-driven waveform inside the grille |
| `left` | `-----` | Preserve the current mouth; gaze affects only the eyes |
| `right` | `-----` | Preserve the current mouth; gaze affects only the eyes |
| `up` | `-----` | Preserve the current mouth; gaze affects only the eyes |
| `down` | `-----` | Preserve the current mouth; gaze affects only the eyes |
| `center` | `-----` | Preserve the current mouth; center only the eyes |
| `neutral` | `-----` | Resting tooth grille |
| `sarcastic` | `~~---` | Asymmetric raised smirk |
| `angry` | `#####` | Tight, clenched tooth grille |
| `happy_fake` | `=====` | Deliberately broad and artificial grin |
| `suspicious` | `--_--` | Small off-center dip or skeptical smirk |
| `tired` | `.....` | Dim, shallow mouth with reduced visual energy |
| `asleep` | `_____` | Black panel during sleep; an optional dim closed line may be used during transition |
| `surprised` | ` O ` | Small rounded open mouth |
| `bored` | `-----` | Flat resting grille with minimal motion |
| `dramatic` | `#####` | Large open mouth or exaggerated pulse |
| `watch` | `=====` | Stable, alert grille |
| `party` | `\___/` | Animated wide smile |
| `error` | `!!!!!` | Red warning or jagged mouth, without rapid flashing |
| `battery_low` | `.._..` | Dim sagging mouth |
| `sunny` | `\___/` | Upturned grin |
| `rainy` | `~~~~~` | Slow wavy or downturned mouth |
| `cloudy` | `(___)` / `(____)` | Soft, subdued oval |
| `stormy` | `ZZZZZ` | Angular zigzag mouth |
| `snowy` | `.....` | Sparse, gentle dot motion |

The two Workflow implementations differ only in the number of underscores in
the `cloudy` ASCII hint. The semantic state is the same.

## Transient aliases and actions

These names exist at other system boundaries and map onto the canonical
states:

| Input | Canonical behavior |
| --- | --- |
| `listening` | Preserve the base mouth while the eyes show attention |
| `thinking_audio` | Preserve the base mouth; eyes indicate audio processing |
| `thinking_long` | Preserve the base mouth; eyes indicate prolonged work |
| `happy` | Alias of `happy_fake` |
| `mouth.talk` | Enter `speaking`, driven by audio intensity |
| `mouth.smirk` | Enter or set `sarcastic` |

## State composition and precedence

Face control needs three independent pieces of state:

1. A persistent expression such as `neutral`, `sarcastic`, or `angry`.
2. A transient activity such as listening, thinking, using a tool, speaking,
   or reporting an error.
3. Eye gaze and tracking, which never alter the mouth by themselves.

The mouth applies the following precedence:

1. `asleep` or system display-off;
2. safety/status overlays such as `error` and `battery_low`;
3. active `speaking` waveform;
4. the persistent expression.

When a transient activity ends, the mouth restores the persistent expression
instead of always returning to neutral.

## Protocol status

The current ESP-NOW protocol exposes only the legacy IDs `0x00` through
`0x09`: idle, listening, thinking, thinking-audio, thinking-long, speaking,
happy, angry, error, and sleep. It cannot uniquely represent all canonical
Workflow expressions or gaze-independent composition.

Before implementing the target-only visuals above, the Brain and mouth should
share a versioned face-state contract that carries:

- persistent expression;
- transient activity;
- speaking intensity;
- transition duration or phase when needed.

Unknown future states must fall back to `neutral` without blanking the panel.
