# ESP32-S3 64x32 HUB75 matrix test

This Arduino C++ firmware tests the custom ESP32-S3-MINI-1-N8 driver PCB
with one 64x32, 1/16-scan HUB75 RGB panel.

It continuously repeats:

1. Red, green, blue, and white screens.
2. A short black screen.
3. Eight vertical color bars.
4. Red, green, and blue horizontal rows.
5. An RGBW checkerboard.
6. A border, grid, diagonals, and four uniquely colored corners.

The test uses a conservative brightness of 64/255. Change `kBrightness` in
`main.cpp` if needed.

## Driver PCB pin map

The schematic connects the ESP32-S3 to the HUB75 connector through two
SN74AHCT245 level shifters:

| HUB75 signal | ESP32-S3 GPIO |
| --- | ---: |
| R1 | 1 |
| G1 | 2 |
| B1 | 3 |
| R2 | 5 |
| G2 | 4 |
| B2 | 6 |
| A | 8 |
| B | 7 |
| C | 10 |
| D | 9 |
| STROBE / LAT | 11 |
| CLK | 12 |
| OE- | 13 |

GPIO3 is a boot-strapping pin. The AHCT245 input should not drive it, but
unexpected boot failures warrant checking GPIO3 with an oscilloscope during
reset.

## Build and upload

Install [PlatformIO](https://platformio.org/), connect the driver PCB, and
run:

```sh
pio run
pio run --target upload
pio device monitor
```

The dependency on `ESP32-HUB75-MatrixPanel-DMA` is declared in
`platformio.ini`.

## Hardware checks

- The PCB and both SN74AHCT245 level shifters require 5 V.
- The matrix needs its separate high-current 5 V power connector; the
  16-pin HUB75 signal connector does not supply panel power.
- PCB ground, panel ground, and power-supply ground must be common.
- Connect the PCB to the matrix's HUB75 **input**, not its output.
- This configuration assumes a conventional 64x32, 1/16-scan panel.

Do not power the LED matrix from the ESP32-S3's 3.3 V rail.
