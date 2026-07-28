# Eye animation assets

`eye_assets.json` is the editable source of truth for the emulator and RP2040
firmware. It contains 240x240 palette-indexed artwork for both the left and
right eye, one animation entry for every protocol state, frame timing, and
loop or ping-pong playback metadata.

Use `../emulator.py` for visual editing. The generated
`../generated/eye_assets.hpp` file is deterministic firmware data and must not
be edited by hand.

`../create_default_assets.py --force` recreates the original procedural
Bender-style artwork. It overwrites manual edits, so it is intended only as a
reset/bootstrap tool.
