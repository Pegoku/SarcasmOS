# SarcasmOS Brain Firmware

ESP-IDF firmware for the ESP32-S3 brain PCB.

## Features

- Enables the `+5V` and `5VHP` rails at boot.
- Uses I2C on `IO8`/`IO9` at 400 kHz by default.
- Controls the left eye at I2C `0x30`, the right eye at I2C `0x31`, and the
  mouth through acknowledged ESP-NOW unicast.
- Coordinates animation transitions: both eyes receive a transition token and
  finish their outgoing animation independently, report READY, then receive a
  back-to-back `CMD_SYNC` commit with a 50 ms future start deadline. The mouth
  is released after the committed eyes confirm activation. If one eye is
  absent or times out, the ready eye is committed and the transition is marked
  degraded instead of remaining frozen.
- Implements the packet protocol from `PCB/FIRMWARE_PLAN.md`, including CRC-8.
- Starts Wi-Fi station mode when configured.
- Serves `GET /api/status` and `POST /api/command`.
- Provides a three-page USB console in the regular firmware for tests, manual
  interaction, and persistent configuration.
- Runs the ICS-43434 and MAX98357A in full-duplex 32-bit Philips I2S mode.
- Streams microphone PCM to the Workflow backend and streams Bender WAV
  responses to the speaker without requiring PSRAM.
- Supports wake-phrase recognition, random acknowledgements, silence
  detection, STT, multi-round LLM tools, progress TTS, final TTS, and device
  conversation history.
- Drives listening/thinking/tool/speaking face states. Real weather results
  select the matching weather face and write the temperature to the Mouth.

## Build

```sh
idf.py set-target esp32s3
idf.py menuconfig
idf.py build
idf.py flash monitor
```

This PCB has 16 MB flash. The project uses [`partitions.csv`](partitions.csv)
with a 4 MB factory partition.

Set optional defaults under `SarcasmOS Brain` in `menuconfig`. Copy the station
MAC printed by the mouth firmware into `ESP-NOW mouth station MAC`. If the
Brain does not join an access point, configure both boards for the same fixed
ESP-NOW channel. If it does join Wi-Fi, build the mouth for that access point's
2.4 GHz channel.

## Regular firmware USB console

The normal firmware accepts input through USB serial. Input is echoed
immediately and one prompt is printed per command. Enter `h` at `brain>`:

```text
1  Testing
2  Interact
3  Configuration
```

The Testing page scans I2C, reads both Eyes, pings the ESP-NOW Mouth, reports
network and power status, plays a speaker tone, and measures the microphone.
The Interact page sends face states, changes brightness, controls both bucks,
and starts a voice request with `listen`.

Configuration is stored in NVS and overrides build-time defaults, including
when those defaults are blank:

```text
set ssid MyNetwork
set password MyPassword
set url http://192.168.1.20:8001
set token a-long-random-device-token
set wake oye bender
set wake-enabled on
set silence-ms 5000
set vad 1200
apply-wifi
```

Use the backend machine's LAN address, not `localhost`. Passwords and tokens
are never printed by `show`. Wi-Fi changes can be applied immediately; other
settings are picked up by the next interaction.

## AI Workflow setup

Run the backend in `../AI/Workflow/SarcasmOS-web` and configure its existing
STT, LLM, and MiniMax/Bender TTS providers. Also set:

```text
DEVICE_API_TOKEN=the-same-long-random-token
DEVICE_USER_EMAIL=the-authorized-sarcasmos-user@example.com
```

The Brain stores only this device credential. Provider keys and Google
Calendar credentials remain on the server. See the Workflow
[`README.md`](../AI/Workflow/SarcasmOS-web/README.md) for startup and endpoint
details.

Wake recognition is server-side: local VAD waits for speech, uploads a short
phrase for STT, and compares it with the configured wake phrase. This avoids
embedding a WakeNet model, but each spoken wake attempt uses one transcription
request and requires Wi-Fi. Leave `wake-enabled` off to use only manual
`listen`.

After a wake match, the Brain plays one randomly selected Bender TTS
acknowledgement, then records the request until the configured silence interval
(5 seconds by default). Tool calls stream related Bender progress phrases
while they run. Weather results pass the real temperature to Mouth `SET_PARAM`
key `2`; fabricated temperatures are never displayed.

Normal face changes use a one-element overwrite queue. Rapid requests replace
an obsolete pending target, Eye protocol-v2 status is polled every 20 ms, and
the mouth receives the target only after both eyes report the matching active
animation/token with no pending animation. The default barrier timeout is
2500 ms and the mouth blend duration is 200 ms; both are configurable under
`SarcasmOS Brain`. Error, sleep, and battery-critical animations explicitly
bypass the graceful barrier.

If an Eye does not reach READY or confirm activation before the first barrier
timeout, Brain resends the request to both Eyes with a fresh transition token
and repeats the complete READY/commit barrier once. The mouth does not receive
the animation until that retry finishes or also times out.

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
