# Eye animation assets

`eye_assets.json` is the editable source of truth for the emulator and RP2040
firmware. It contains paired 240x240 palette-indexed artwork for both eyes, one
animation entry for every protocol state, frame timing, independent left/right
orientation toggles, and loop or ping-pong playback metadata.

Use `../emulator.py` for visual editing. The generated
`../generated/eye_assets.hpp` file is deterministic firmware data and must not
be edited by hand.

In the emulator, select a state and frame, use the Left/Both/Right buttons or
Tab to choose the view, then press `E` to edit one frame or `O` to edit the
complete animation in GIMP. In the combined view, editing uses the left eye.
Overwriting an exported PPM automatically imports it. Whole-animation folders
use names such as `angry-left-frame-01.ppm`; adding, deleting, or renumbering
these files changes the animation sequence and regenerates the opposite eye
as its horizontal mirror.

Use `Flip left eye` and `Flip right eye` to toggle each displayed orientation
independently. This is useful for directional gaze and other states that
should not use the default mirrored presentation. The buttons change the
emulator immediately and save the setting without compiling; the next normal
firmware build applies it to the generated frame pairs.

The pack supports `loop` and `ping_pong` playback. Frame timing is stored per
animation. Adding or removing a frame always changes the paired sequence, so
the two eye boards remain synchronized.

Press `F` to edit the animation's frame timeline. Timeline rows are references
to stored sprite pairs: copying a row or repeating a selected effect range does
not copy its bitmap data. Rows can be reordered, reversed, deleted, or expanded
with a reverse exit, and the editor can switch the animation between loop and
ping-pong playback. Saving rewrites only the selected asset pack.

`../create_default_assets.py --force` recreates the original procedural
Bender-style artwork. It overwrites manual edits, so it is intended only as a
reset/bootstrap tool.
