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
- Runs the complete assistant orchestration on the ESP32. There is no
  SarcasmOS backend process to run on another computer.
- Serves the device and AI API itself: `GET /api/status`,
  `GET /api/config`, `POST /api/command`, `POST /api/ai/text`, and
  `POST /api/ai/listen`.
- Provides a three-page USB console in the regular firmware for tests, manual
  interaction, and persistent configuration.
- Runs the ICS-43434 and MAX98357A in full-duplex 32-bit Philips I2S mode.
- Streams microphone PCM directly from the ESP32 to the configured STT
  provider and streams Bender WAV responses directly to the speaker without
  requiring PSRAM.
- Runs wake-phrase recognition, random acknowledgements, silence detection,
  STT, the multi-round LLM/tool loop, short progress TTS before every tool,
  final TTS, and bounded in-RAM conversation memory on the ESP32.
- Calls weather, time, Google Calendar, robot status, LLM, STT, and TTS
  services directly from the ESP32 over TLS.
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
It also gates the live microphone WebSocket:

```text
stream on
stream status
mic gain 2.00
mic sensitivity 600
stream off
```

Enter `p` on the Testing page for the interactive speaker controller. Use
Up/Down or `W`/`S` to choose approximately 0.05 W through 3.2 W, Space/Enter
to preview a one-second tone, `A` to apply and save that level as the default
for tests and TTS, and `Q`/Esc to return. Levels above 1.5 W require explicit
confirmation. Power estimates assume a 4-ohm speaker, 5 V amplifier supply,
and the PCB's fixed 9 dB MAX98357A gain. `GAIN_SLOT` is not connected to the
ESP32, so firmware changes digital amplitude/dBFS rather than hardware gain.

The `/api/audio/mic` route does not exist until `stream on` is entered. Turning
it off unregisters the route and disconnects its client. This setting is
intentionally runtime-only and returns to disabled after a reboot.
Microphone gain and sensitivity are persistent and shared by the level test,
live stream, wake detection, and STT capture. Gain accepts `0.25` to `8.00`;
larger values make the PCM louder but also amplify noise and may clip. The
sensitivity value is the VAD amplitude threshold, so a lower value detects
quieter speech.

The Interact page sends face states, changes brightness, controls both bucks,
starts a microphone request with `listen`, and accepts a typed request with
`ask <message>`.

Configuration is stored in NVS and overrides build-time defaults, including
when those defaults are blank:

```text
show
view stt-model
view mic-gain
set ssid MyNetwork
set password MyPassword
set ai-token your-shared-Hack-Club-AI-token
set llm-token optional-different-LLM-token
set replicate-token optional-different-STT-TTS-token
set llm-url https://ai.hackclub.com/proxy/v1
set replicate-url https://ai.hackclub.com/proxy/v1/replicate
set llm-model ~anthropic/claude-sonnet-latest
set stt-model vaibhavs10/incredibly-fast-whisper:VERSION
set tts-model minimax/speech-2.8-turbo
set voice-id your-Minimax-Bender-voice-id
set calendar-token optional-Google-OAuth-access-token
set timezone Europe/Madrid
set wake oye bender
set wake-enabled on
set silence-ms 5000
set vad 1200
set mic-gain 1.00
apply-wifi
```

The specific LLM and Replicate tokens fall back to `ai-token` when blank.
Passwords and tokens are never printed by `show`. Wi-Fi changes can be
applied immediately; AI settings are picked up by the next interaction.
Configuration is stored in NVS. Enable NVS encryption for a production unit
because provider and Calendar tokens otherwise remain plaintext in flash.

## ESP32-native AI workflow

The runtime architecture is:

```text
microphone -> ESP32 Brain -> STT -> LLM/tool loop -> Bender TTS -> speaker
                         |-> Eyes over I2C
                         |-> Mouth over ESP-NOW
USB CLI / local HTTP ----^
```

The STT/LLM/TTS providers are cloud services, but the ESP32 is the client,
workflow engine, hardware controller, and local HTTP server. The Python code
under `../AI/Workflow` is not a runtime dependency.

Local VAD waits for speech before opening the HTTPS STT request. The ESP32
compares the returned transcript with the configured wake phrase. This avoids
embedding a WakeNet model, but each spoken candidate uses one transcription
request and requires Wi-Fi. Leave `wake-enabled` off to use only manual
`listen`.

After a wake match, the Brain plays one randomly selected Bender TTS
acknowledgement, then records the request until the configured silence interval
(5 seconds by default). Tool calls stream related Bender progress phrases
while they run. Weather results pass the real temperature to Mouth `SET_PARAM`
key `2`; fabricated temperatures are never displayed.

The native tools are:

- `get_weather`, using `wttr.in`;
- `get_time`, using the configured IANA timezone;
- `robot_status`, using live Eye, Mouth, network, microphone, and speaker
  state from this firmware;
- `google_calendar_search`, using the optional OAuth access token stored in
  NVS.

Google access tokens can expire. Replace `calendar-token` when Google returns
an expired-permission response.

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

## ESP32 HTTP API

The API is served by the ESP32 at `http://DEVICE_IP/`:

```sh
curl http://DEVICE_IP/api/status
curl http://DEVICE_IP/api/config
curl -X POST http://DEVICE_IP/api/command \
  -H 'Content-Type: application/json' \
  -d '{"animation":"speaking"}'
curl -X POST http://DEVICE_IP/api/ai/text \
  -H 'Content-Type: application/json' \
  -d '{"message":"¿Qué tiempo hace en Madrid?"}'
curl -X POST http://DEVICE_IP/api/ai/listen
```

`/api/ai/text` runs the complete model/tool/TTS workflow and returns the final
answer as JSON after it has played it. `/api/ai/listen` records from the
device microphone and does the same. Only one voice workflow can own the
microphone/speaker at a time.

### Live microphone WebSocket

After enabling it from the Testing page, connect to:

```text
http://DEVICE_IP/api/audio/mic
```

That URL serves a small built-in player. Press **Start listening** to satisfy
the browser's audio permission/autoplay requirement. A custom website can
connect directly to:

```text
ws://DEVICE_IP/api/audio/mic
```

The page also exposes persistent gain and speech-threshold controls. They take
effect immediately after **Apply and save** and stay synchronized with values
changed from USB serial. The first WebSocket message is JSON describing the
stream and current settings. Following messages are binary frames containing
signed little-endian PCM with this fixed format:

```json
{"format":"pcm_s16le","sample_rate":16000,"channels":1,"frame_samples":1600,"gain_q8":256,"gain":1.00,"vad_threshold":1200}
```

A custom WebSocket client can update both values with a text frame. Values are
saved in NVS, and the server replies with the resulting settings:

```json
{"gain_q8":512,"vad_threshold":600}
```

That is one 100 ms, 3,200-byte frame under normal operation. A browser should
set `binaryType = "arraybuffer"`, convert each message to `Int16Array`, scale
samples by `1 / 32768`, and queue them in an `AudioWorklet` running at the
browser audio context's sample rate. Only one WebSocket listener is accepted.
Wake detection is suspended while the endpoint is enabled, and the stream has
exclusive microphone/audio access while a client is connected.

## PCB self-test firmware

An independent bring-up firmware is available in [`self_test/`](self_test/).
It reports Wi-Fi, buck-control, I2C power-management and eye devices, the
ESP-NOW mouth, W5500, GPIO, and I2S audio checks through native USB serial, then
opens an interactive menu for manual power, speaker, microphone, and individual
eye/mouth visual tests. See
[`self_test/README.md`](self_test/README.md) for build, flash, and test details.
