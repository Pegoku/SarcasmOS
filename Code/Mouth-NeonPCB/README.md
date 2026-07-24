# ESP32-S3 64x32 HUB75 matrix test

This Arduino C++ firmware tests an Adafruit MatrixPortal S3 with one 64x32,
1/16-scan HUB75 RGB panel.

It continuously repeats:

1. Red, green, blue, and white screens.
2. A short black screen.
3. Eight vertical color bars.
4. Red, green, and blue horizontal rows.
5. An RGBW checkerboard.
6. A border, grid, diagonals, and four uniquely colored corners.

The onboard red LED toggles at every test step so firmware activity remains
visible even if the panel is disconnected. The test uses a conservative
brightness of 64/255. Change `kBrightness` in
`main.cpp` if needed.

## Build and upload

Install [PlatformIO](https://platformio.org/), connect the MatrixPortal S3,
and run:

```sh
pio run
pio run --target upload
pio device monitor
```

The dependency on `ESP32-HUB75-MatrixPanel-DMA` is declared in
`platformio.ini`.

## Important hardware assumptions

The GPIO map in `main.cpp` is for the Adafruit MatrixPortal S3. A generic
ESP32-S3 board does not have standard `D6`, `A5`, and similar pin aliases. If
using another board, update all HUB75 GPIO constants near the top of
`main.cpp` to match the wiring.

Power the panel from a suitable 5 V supply and connect the panel ground to
the ESP32-S3 ground. Do not power a full LED matrix from the ESP32-S3's 3.3 V
pin.
