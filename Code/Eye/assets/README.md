# Eye animation assets

`eye_assets.json` is the editable source of truth for the emulator and RP2040
firmware. It contains paired 240x240 palette-indexed artwork for both eyes, one
animation entry for every protocol state, frame timing, eye flip direction,
and loop or ping-pong playback metadata.

Use `../emulator.py` for visual editing. The generated
`../generated/eye_assets.hpp` file is deterministic firmware data and must not
be edited by hand.

In the emulator, select a state and frame, use the Left/Both/Right buttons or
Tab to choose the view, then press `E` to edit one frame or `O` to edit the
complete animation in GIMP. In the combined view, editing uses the left eye.
Overwriting an exported PPM automatically imports it. Whole-animation folders
use names such as `angry-left-frame-01.ppm`; adding, deleting, or renumbering
these files changes the animation sequence and regenerates the opposite eye
according to the animation's pairing mode.

Use `Flip right from left` when the left eye is the source, or `Flip left from
right` when the right eye is the source. `No flip` copies the currently
selected eye to both sides. This is useful for directional gaze and any other
state where both displays should use identical pixels. The setting is stored
per animation and is respected by later frame and folder imports.

The pack supports `loop` and `ping_pong` playback. Frame timing is stored per
animation. Adding or removing a frame always changes the paired sequence, so
the two eye boards remain synchronized.

`../create_default_assets.py --force` recreates the original procedural
Bender-style artwork. It overwrites manual edits, so it is intended only as a
reset/bootstrap tool.
