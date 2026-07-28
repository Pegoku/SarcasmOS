# SarcasmOS Eye Firmware

Pico SDK C++ firmware for each RP2040 eye board.

The complete Workflow-derived visual and state contract is documented in
[`EYE_STATES.md`](EYE_STATES.md).

## Animation editor

The desktop emulator and the RP2040 firmware use the same palette-indexed
240x240 animation pack:

```sh
python emulator.py
```

The preview is clipped to the GC9A01's round screen. Each animation frame
uses flat Bender-style artwork with a square pupil. Each animation stores an
independent left- and right-eye orientation toggle. Weather states replace the
eye completely with a full-screen weather symbol.

Main controls:

- Left/Right: select one of the 31 states.
- Up/Down: select a frame and pause playback.
- Left eye/Both eyes/Right eye buttons: choose the preview layout.
- Tab: cycle through left, both side by side, and right views.
- Flip left eye: toggle horizontal flipping for the left display.
- Flip right eye: toggle horizontal flipping for the right display.
- Space or A: play/pause.
- E: open the current native 240x240 frame in GIMP.
- O: open every frame of the current eye/animation in GIMP.
- Insert/Delete: add or remove a paired animation frame.
- T: change the current animation's frame time.
- S: synchronize an existing animation edit folder.
- C: copy the current native frame to the image clipboard.
- R: reload the asset pack.

Saving an emulator-exported PPM over the same file makes the emulator import
it automatically, update `assets/eye_assets.json`, regenerate
`generated/eye_assets.hpp`, and reload the preview. New exact colors occupy
free slots in the shared 16-color palette; once full, imports use the nearest
palette color. Orientation buttons update the preview immediately and save
only their two small per-animation settings; firmware assets are regenerated
during the next normal build.

Headless validation and command-line frame exchange are also available:

```sh
python emulator.py --self-test
python emulator.py --state angry --view both --gap 24 --dump angry-pair.ppm
python emulator.py --state angry --view right --dump angry-right.ppm
python asset_tool.py export happy_fake happy-left.ppm --role left --frame 2
python asset_tool.py import happy_fake happy-left.ppm --role left --frame 2
python asset_tool.py validate
python asset_tool.py compile
```

An alternate emulator-compatible pack containing the 44 animations extracted
from the external procedural ESP32 sketch can be previewed without replacing
the production assets:

```sh
python emulator.py --assets "new animations/eye_assets.json" --view both
```

Alternate packs are view-only; the editor continues to write only to the
production `assets/eye_assets.json` pack.

`create_default_assets.py --force` recreates the initial procedural artwork
and destroys manual sprite edits. Normal CMake builds regenerate only the C++
header when the editable JSON or compiler changes.

## Build

Left eye:

```sh
cmake -S . -B build-left -DDEVICE_ROLE=0 -DI2C_ADDRESS=0x30
cmake --build build-left
```

Right eye:

```sh
cmake -S . -B build-right -DDEVICE_ROLE=1 -DI2C_ADDRESS=0x31
cmake --build build-right
```

Display test:

```sh
cmake -S . -B build-test
cmake --build build-test --target sarcasmos_eye_display_test
```

The display test cycles every three seconds through black, white, red, green,
blue, vertical and horizontal color bars, a checkerboard, an alignment screen,
a grayscale gradient, and a two-axis RGB gradient.

Animation autoplay test:

```sh
cmake -S . -B build-animation-test \
  -DANIMATION_AUTOPLAY=ON \
  -DDEVICE_ROLE=0 \
  -DI2C_ADDRESS=0x30
cmake --build build-animation-test --target sarcasmos_eye
```

This is the real eye firmware with automatic animation selection enabled. It
shows each of the 31 animation states for three seconds, completing a cycle in
93 seconds. Normal builds leave autoplay disabled and continue to select
animations through I2C.

## Upload with J-LinkOB

The old J-LinkOB firmware cannot configure the RP2040 through J-Link Commander,
but OpenOCD can access core 0 directly. From this directory, upload the display
test with:

```sh
OPENOCD_ROOT=/home/pegoku/.espressif/tools/openocd-esp32/v0.12.0-esp32-20260424/openocd-esp32
EYE_IMAGE=build-test/sarcasmos_eye_display_test.elf
"${OPENOCD_ROOT}/bin/openocd" \
  -s "${OPENOCD_ROOT}/share/openocd/scripts" \
  -f interface/jlink.cfg \
  -c "adapter speed 100" \
  -c "set USE_CORE 0" \
  -f target/rp2040.cfg \
  -c "program ${EYE_IMAGE} verify reset exit"
```

Use `EYE_IMAGE=build-left/sarcasmos_eye.elf` or
`EYE_IMAGE=build-right/sarcasmos_eye.elf` to upload the normal left- or
right-eye firmware. Use
`EYE_IMAGE=build-animation-test/sarcasmos_eye.elf` for the animation autoplay
test. OpenOCD verifies the flash and resets the RP2040; the RUN and BOOT buttons
are not required.

## Notes

- I2C slave uses `GPIO4`/`GPIO5`.
- Display SPI uses `GPIO17`/`GPIO18`/`GPIO19` plus `DC=GPIO20`, `RST=GPIO21`, `BL=GPIO22`.
- The round 240x240 RGB565 display uses a GC9A01A controller.
