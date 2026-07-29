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

Hover over any button to highlight it and show an explanation of its action.

- Left/Right: select one of the 31 states.
- Up/Down: select a frame and pause playback.
- Left eye/Both eyes/Right eye buttons: choose the preview layout.
- Tab: cycle through left, both side by side, and right views.
- Flip left eye: toggle horizontal flipping for the left display.
- Flip right eye: toggle horizontal flipping for the right display.
- Space: play or pause the current animation without changing states.
- A: toggle full automatic playback and scrolling through animation states.
- E: open the current native 240x240 frame in GIMP.
- O: open every frame of the current eye/animation in GIMP.
- F: open the frame timeline editor.
- Insert/Delete: add or remove a paired animation frame.
- T: change the current animation's frame time.
- S: synchronize an existing animation edit folder.
- C: copy the current native frame to the image clipboard.
- R: reload the asset pack.

The frame timeline editor can reorder a multi-frame selection, copy entries,
repeat an effect range, reverse a selection, or append the existing sequence
in reverse as an exit. Copied and repeated entries point to the same stored
left/right sprites, so they add playback steps without duplicating bitmap
data. Its side-panel preview follows the selected row; Left/Right step through
frames, and its Play button previews the unsaved working sequence without
starting state auto-scroll. The editor also selects loop or ping-pong playback
and supports the same production and alternate asset packs.

`Repeat selection…` can either add a fixed number of shared-reference repeats
or mark one contiguous range to repeat until the controller changes animation
state. An until-end range has its own Regular/Ping-pong toggle: intro frames
before the range play once, the selected effect then repeats indefinitely, and
the next state command requests its exit. Timeline rows after the persistent
range then play once as an outro before the pending state begins. When a loop
occupies the final pose of a whole-animation ping-pong, its entry frames become
the automatic return outro. The timeline marks range rows with `↔`.
The left-side Animation actions panel lists each persistent loop. Its Delete
button removes the action without deleting frames or artwork; double-clicking
the action text opens it in edit mode so its range and loop direction can be
changed. Save the timeline to persist action edits or deletions.

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

An alternate emulator-compatible pack maps animations extracted from the
external procedural ESP32 sketch onto the production pack's same 31 state IDs
and order. It can be previewed without replacing the production assets:

```sh
python emulator.py --assets "new animations/eye_assets.json" --view both
```

Alternate packs support the same frame and timeline editing, GIMP auto-import,
add/remove, timing, folder synchronization, and orientation controls as the
production pack. Changes are written only to the path passed through
`--assets`; editing an alternate pack does not regenerate the production
firmware header. Independent left/right artwork is preserved when only one
alternate-pack eye is edited.

The ready-made PNG animation set is also available as a lossless converted
31-state pack:

```sh
python emulator.py --assets "png animations/eye_assets.json" --view both
```

Its original PNG sources and reproducible converter are kept together under
`png animations/`.

`create_default_assets.py --force` recreates the initial procedural artwork
and destroys manual sprite edits. Normal CMake builds regenerate only the C++
header when the editable JSON or compiler changes.

## Build

The helper script can build, upload, and then monitor all three firmware types
for either eye:

```sh
./flash.sh --left --regular --build --upload --swd-monitor
./flash.sh --left --self-test --build --upload --swd-monitor
./flash.sh --right --demo --build --upload --swd-monitor
./flash.sh --right --demo --assets "png animations/eye_assets.json" --build --upload
```

`--regular` is the default and receives animation commands over I2C.
`--self-test` cycles through display patterns, while `--demo` cycles through all
31 animations and prints each animation's name and ID. Regular firmware also
reports every I2C-driven animation change. Use `--monitor` for UART or
`--swd-monitor` to stream the same messages non-intrusively over SWD using RTT;
the SWD monitor needs no serial adapter and keeps the RP2040 running.
Actions, firmware type, and the eye selector can appear in any order. The UART
monitor automatically uses the only connected `/dev/serial/by-id`,
`/dev/ttyACM`, or `/dev/ttyUSB` device. If more than one is connected, select
one with `--port /dev/ttyACM0` or set `EYE_LEFT_PORT` and `EYE_RIGHT_PORT`.
Run `./flash.sh --help` for all options and environment overrides.

Every activated animation now starts at frame one on its own local clock; the
I2C sync timestamp is never added to its frame index. A request for another
state becomes the current animation's end signal. Regular playback completes
its forward pass, whole-animation ping-pong completes its return to frame one,
and a persistent range loop runs its stored outro before the pending state is
activated. If a range occupies the final frames and the animation itself is
ping-pong, firmware automatically returns through the entry frames. Therefore
demo states remain active for at least three seconds and may take slightly
longer while their graceful exit finishes. Regular I2C mode uses the identical
transition logic.

Use `--assets PATH` to compile regular or demo firmware with another compatible
asset pack. The generated header stays inside that firmware's build directory,
so building an alternate pack does not overwrite `generated/eye_assets.hpp` or
the production asset source. Upload-only commands verify that the selected pack
path matches the image configuration and ask for a rebuild if it does not.

All three firmware modes rotate the display output 180 degrees while retaining
the correct mirrored orientation for each eye.

The equivalent manual build commands are below.

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
