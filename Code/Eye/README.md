# SarcasmOS Eye Firmware

Pico SDK C++ firmware for each RP2040 eye board.

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
cycles through idle, thinking, speaking, happy, angry, error, and sleep in
23 seconds. Normal builds leave autoplay disabled and continue to select
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
