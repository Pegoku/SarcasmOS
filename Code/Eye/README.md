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

## Notes

- I2C slave uses `GPIO4`/`GPIO5`.
- Display SPI uses `GPIO17`/`GPIO18`/`GPIO19` plus `DC=GPIO20`, `RST=GPIO21`, `BL=GPIO22`.
- LH128 display output uses the 240x240 RGB565 ST7789-compatible SPI init and rendering path.
