# SarcasmOS Brain Firmware

ESP-IDF firmware for the ESP32-S3 brain PCB.

## Features

- Enables the `+5V` and `5VHP` rails at boot.
- Uses I2C on `IO8`/`IO9` at 400 kHz by default.
- Controls display slaves at `0x30` left eye, `0x31` right eye, and `0x32` mouth.
- Implements the packet protocol from `PCB/FIRMWARE_PLAN.md`, including CRC-8.
- Starts Wi-Fi station mode when configured.
- Serves `GET /api/status` and `POST /api/command`.
- Initializes the MAX98357A/ICS-43434 shared I2S pins for audio bring-up.

## Build

```sh
idf.py set-target esp32s3
idf.py menuconfig
idf.py build
idf.py flash monitor
```

Set Wi-Fi credentials under `SarcasmOS Brain` in `menuconfig`.

Example API command:

```sh
curl -X POST http://DEVICE_IP/api/command -d '{"animation":"speaking"}'
```

## PCB self-test firmware

An independent bring-up firmware is available in [`self_test/`](self_test/).
It reports Wi-Fi, buck-control, I2C power-management devices, display
controllers, W5500, GPIO, and I2S audio checks through native USB serial, then
opens an interactive menu for manual power, speaker, microphone, and individual
eye/mouth visual tests. See
[`self_test/README.md`](self_test/README.md) for build, flash, and test details.
