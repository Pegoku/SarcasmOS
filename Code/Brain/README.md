# SarcasmOS Brain Firmware

ESP-IDF firmware for the ESP32-S3 brain PCB.

## Features

- Enables the `+5V` and `5VHP` rails at boot.
- Uses I2C on `IO8`/`IO9` at 400 kHz by default.
- Controls the left eye at I2C `0x30`, the right eye at I2C `0x31`, and the
  mouth through acknowledged ESP-NOW unicast.
- Coordinates animation transitions: both eyes receive a transition token and
  finish their outgoing animation before the matching mouth blend is released.
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

Set Wi-Fi credentials under `SarcasmOS Brain` in `menuconfig`. Copy the station
MAC printed by the mouth firmware into `ESP-NOW mouth station MAC`. If the
Brain does not join an access point, configure both boards for the same fixed
ESP-NOW channel. If it does join Wi-Fi, build the mouth for that access point's
2.4 GHz channel.

Normal face changes use a one-element overwrite queue. Rapid requests replace
an obsolete pending target, Eye protocol-v2 status is polled every 20 ms, and
the mouth receives the target only after both eyes report the matching active
animation/token with no pending animation. The default barrier timeout is
2500 ms and the mouth blend duration is 200 ms; both are configurable under
`SarcasmOS Brain`. Error, sleep, and battery-critical animations explicitly
bypass the graceful barrier.

`GET /api/status` includes `face_transition`, detailed active/pending Eye
status, and Mouth transition token/progress. Mouth application-v2 status is
accepted during migration but does not provide blend progress. Eye protocol-v1
status cannot satisfy the barrier and therefore produces a logged degraded
transition after the timeout.

Example API command:

```sh
curl -X POST http://DEVICE_IP/api/command -d '{"animation":"speaking"}'
```

## PCB self-test firmware

An independent bring-up firmware is available in [`self_test/`](self_test/).
It reports Wi-Fi, buck-control, I2C power-management and eye devices, the
ESP-NOW mouth, W5500, GPIO, and I2S audio checks through native USB serial, then
opens an interactive menu for manual power, speaker, microphone, and individual
eye/mouth visual tests. See
[`self_test/README.md`](self_test/README.md) for build, flash, and test details.
