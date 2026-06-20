# SarcasmOS Mouth Firmware

Pico SDK C++ firmware for the RP2040 HUB75/HUB75E mouth board.

## Build

```sh
cmake -S . -B build -DI2C_ADDRESS=0x32
cmake --build build
```

## Notes

- I2C slave uses `GPIO4`/`GPIO5`.
- Matrix pins match the PCB docs: RGB data on `GPIO6`-`GPIO11`, `E/A/B/C/D` on `GPIO12`-`GPIO16`, `CLK=GPIO17`, `LAT=GPIO18`, `OE=GPIO19`.
- Output defaults to a 128x64, 1/32-scan HUB75E panel with a 4-bit-per-channel scan loop.
- `SET_PARAM` with key `1` updates speaking mouth intensity.
