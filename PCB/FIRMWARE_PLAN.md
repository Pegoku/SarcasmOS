# SarcasmOS Firmware Plan

This document defines the planned firmware architecture for the SarcasmOS hardware. It complements `SCHEMATICS_FIRMWARE_INTERFACE.md`, which documents the electrical connections and pin mappings.

## Product Goal

SarcasmOS is a voice-assistant robot with three physical displays:

- Left eye display, using one `eye` PCB.
- Right eye display, using a second `eye` PCB.
- Mouth display, using one `mouth` PCB.

The `brain` PCB runs the main application on an ESP32-S3 using ESP-IDF. It handles voice-assistant processing, networking, web/API control, audio playback/capture, power/peripheral control, and animation coordination. The two eyes and the mouth are slave display controllers. The brain tells them what animation/state to play over I2C.

## Firmware Targets

| Target | MCU | Framework | Role |
|---|---|---|---|
| Brain | ESP32-S3-WROOM-1 | ESP-IDF | Main controller, networking, audio, I2C master, web/API, animation scheduler |
| Left eye | RP2040 | Pico SDK or compatible RP2040 stack | I2C slave, display renderer |
| Right eye | RP2040 | Pico SDK or compatible RP2040 stack | I2C slave, display renderer |
| Mouth | RP2040 | Pico SDK or compatible RP2040 stack | I2C slave, HUB75-style mouth renderer |

## High-Level Runtime Architecture

```text
User / network / web UI
        |
        v
ESP32-S3 brain firmware
  - Wi-Fi/Ethernet networking
  - Local web/API service
  - Voice assistant state machine
  - Audio playback and microphone capture
  - Animation scheduler
  - I2C master commands
        |
        +-- I2C --> Left eye RP2040  --> LH128 display
        +-- I2C --> Right eye RP2040 --> LH128 display
        +-- I2C --> Mouth RP2040     --> RGB matrix / mouth display
```

The existing web project at `Code/AI/Workflow/SarcasmOS-web/` is the behavioral reference for the embedded UI/API. Its current backend exposes chat/audio/status/history/command endpoints and the frontend has animation states like `speaking`, `thinking`, `thinking-audio`, and `thinking-long`. The ESP32 firmware should implement a smaller embedded equivalent appropriate for ESP-IDF resources.

## Brain Firmware Responsibilities

### Core Services

The ESP32-S3 brain firmware should provide these services:

| Service | Purpose | Hardware/interfaces |
|---|---|---|
| System init | Bring up clocks, NVS, event loop, logging, watchdogs | ESP-IDF core |
| Power control | Enable 5 V rails and monitor battery/charger | `5V_EN`, `5VHP_EN`, BQ25792, MAX17049 |
| I2C master | Control eyes, mouth, charger, fuel gauge | `SDA`=`IO8`, `SCL`=`IO9` |
| Display coordinator | Select animations and send commands to slaves | I2C display protocol |
| Audio output | Play TTS/assistant audio | MAX98357A over I2S |
| Audio input | Capture microphone audio | ICS-43434 over I2S |
| Networking | Serve API/UI and reach external AI services if used | Wi-Fi and/or W5500 Ethernet |
| Web/API | Embedded control/status interface | HTTP server |
| Assistant state machine | Coordinate listening/thinking/speaking/errors | Internal app logic |
| Diagnostics | Logs, status LED, health checks | UART0, LED, API |

### Brain ESP-IDF Modules

Recommended module boundaries:

| Module | Responsibilities |
|---|---|
| `app_main` | Boot sequence, task creation, dependency wiring |
| `board_pins` | Central pin definitions from schematic document |
| `power_manager` | Regulator enables, charger/fuel-gauge setup, battery state |
| `i2c_bus` | Shared I2C master driver, bus locking, scan, read/write helpers |
| `display_bus` | High-level I2C protocol for eyes and mouth |
| `animation_manager` | Current expression/animation state and transitions |
| `audio_out` | I2S speaker output through MAX98357A |
| `audio_in` | I2S microphone capture through ICS-43434 |
| `network_manager` | Wi-Fi and/or W5500 Ethernet bring-up and reconnection |
| `web_server` | HTTP API and static UI serving |
| `assistant_core` | Voice assistant state machine and backend/cloud integration |
| `storage` | NVS config: Wi-Fi credentials, I2C addresses, preferences |
| `diagnostics` | Health/status endpoints, logging helpers, reset reasons |

## I2C Device Model

### Bus Participants

All runtime display commands use the brain I2C bus.

| Device | Role | Address plan |
|---|---|---|
| Left eye RP2040 | I2C slave display controller | Unique fixed/configured address |
| Right eye RP2040 | I2C slave display controller | Unique fixed/configured address |
| Mouth RP2040 | I2C slave display controller | Unique fixed/configured address |
| BQ25792 | Charger IC | Datasheet-defined address |
| MAX17049 | Fuel gauge IC | Datasheet-defined address |

Suggested display slave defaults, unless they conflict with the power-management ICs or existing firmware:

| Device | Suggested address |
|---|---:|
| Left eye | `0x30` |
| Right eye | `0x31` |
| Mouth | `0x32` |

These are firmware choices. The schematic has no hardware address-selection pins for the eye boards, so each eye must be provisioned with its own role/address in firmware or persistent configuration.

### Address Provisioning Options

Choose one provisioning method before writing production firmware:

| Option | How it works | Pros | Cons |
|---|---|---|---|
| Separate firmware builds | Flash left-eye and right-eye binaries with different compile-time addresses | Simple and reliable | Must track which binary is flashed to which board |
| NVM role config | Same binary, role stored in RP2040 flash/config sector | One firmware image | Needs provisioning tool or command path |
| I2C claim command | Brain assigns role during setup and slave stores it | Flexible | Needs temporary unique identity or one-at-a-time provisioning |

Recommended first implementation: separate firmware builds or a build-time `DEVICE_ROLE` setting for `left_eye`, `right_eye`, and `mouth`.

## Display I2C Protocol

The display boards should expose a simple register/command protocol. Keep commands small and let each RP2040 render animations locally. The brain should send animation intent, not full framebuffers, except for debug/testing.

### Common Packet Format

Use a compact binary command frame:

| Byte(s) | Field | Description |
|---:|---|---|
| 0 | `version` | Protocol version, start at `0x01` |
| 1 | `command` | Command ID |
| 2 | `sequence` | Incrementing command sequence number |
| 3 | `length` | Payload length in bytes |
| 4..N | `payload` | Command-specific payload |
| N+1 | `crc8` | Optional CRC-8 over prior bytes |

For early bring-up, CRC can be omitted if ESP-IDF/Pico I2C reliability is sufficient at the chosen bus speed. For production, include it to detect malformed animation commands.

### Common Commands

| Command | ID | Payload | Applies to | Purpose |
|---|---:|---|---|---|
| `PING` | `0x01` | none | all | Check device presence |
| `GET_INFO` | `0x02` | none | all | Read role, firmware version, display type |
| `SET_BRIGHTNESS` | `0x10` | `uint8 brightness` | all | Set display/backlight brightness, 0-255 |
| `SET_ANIMATION` | `0x20` | `animation_id`, options | all | Start named animation |
| `SET_EXPRESSION` | `0x21` | `expression_id`, options | all | Set static facial expression |
| `SYNC` | `0x22` | `timestamp_ms` or phase | all | Synchronize animations across displays |
| `STOP` | `0x23` | none | all | Stop current animation / blank display |
| `SET_PARAM` | `0x30` | key/value | all | Adjust animation speed, intensity, etc. |
| `DEBUG_FRAME` | `0x7E` | small display-specific frame data | debug only | Test rendering path |
| `RESET` | `0x7F` | magic value | all | Soft-reset display firmware |

### Suggested Animation IDs

| Animation | ID | Eye behavior | Mouth behavior |
|---|---:|---|---|
| `idle` | `0x00` | Neutral eyes, occasional blink | Neutral mouth/off or idle line |
| `listening` | `0x01` | Focused/awake eyes | Subtle listening indicator |
| `thinking` | `0x02` | Looking around, squint/pulse | Thinking animation |
| `thinking_audio` | `0x03` | Stronger animated attention | Audio processing indicator |
| `thinking_long` | `0x04` | Impatient/hard squint | Long wait animation |
| `speaking` | `0x05` | Blink/expressive speaking state | Mouth viseme/amplitude animation |
| `happy` | `0x06` | Happy eyes | Smile/mouth expression |
| `angry` | `0x07` | Angry/squint eyes | Angry mouth |
| `error` | `0x08` | Error/glitch eyes | Error/glitch mouth |
| `sleep` | `0x09` | Closed/off eyes | Blank/off mouth |

These names intentionally mirror the web prototype animation states where possible.

### Readback Registers

Each slave should support readback after command writes:

| Register/response | Fields |
|---|---|
| Device info | role, firmware version, protocol version, capabilities |
| Status | current animation, busy flag, error code, uptime |
| Last command result | last sequence, ACK/NACK, error code |
| Metrics | dropped commands, render FPS, I2C error count |

## Brain Display Control Flow

The brain should never need to manually render eye or mouth pixels for normal operation. It should run an animation scheduler:

1. Detect high-level assistant state: idle, listening, thinking, speaking, error.
2. Convert state to desired animation per display.
3. Send `SET_ANIMATION` to left eye, right eye, and mouth.
4. Send `SYNC` so all displays align timing.
5. During speaking, send amplitude/viseme parameters to the mouth periodically.
6. On state changes, send new animations only when necessary to avoid I2C spam.

### Speaking Animation

For speech, the brain can coordinate mouth movement in two stages:

| Stage | Behavior |
|---|---|
| Bring-up | Mouth board plays autonomous `speaking` animation at fixed intensity |
| Improved | Brain extracts audio amplitude envelope and sends periodic intensity values |
| Advanced | Brain or TTS pipeline sends viseme/phoneme timing to mouth board |

Recommended first implementation: send `SET_ANIMATION(speaking)` when audio starts and `SET_ANIMATION(idle)` when audio ends. Add amplitude later.

## Eye Firmware Plan

Each eye RP2040 firmware should be identical except for role/address configuration.

### Eye Firmware Responsibilities

| Area | Requirement |
|---|---|
| I2C slave | Listen on configured left/right address and parse display protocol |
| Display driver | Initialize and draw to the `LH128` display over SPI-like pins |
| Animation engine | Render local eye animations from command IDs |
| Backlight | Control `BL_PIN` on `GPIO22`, including brightness/PWM if supported |
| Status LED | Use `GPIO2` for heartbeat/error/debug |
| Watchdog | Recover if animation loop or I2C handling stalls |
| Debug | UART and SWD support through `J3` |

### Eye Pin Use

| Function | RP2040 pin |
|---|---|
| I2C SDA | `GPIO4` |
| I2C SCL | `GPIO5` |
| Display CS | `GPIO17` |
| Display SCK | `GPIO18` |
| Display MOSI | `GPIO19` |
| Display DC | `GPIO20` |
| Display RST | `GPIO21` |
| Backlight | `GPIO22` |
| Status LED | `GPIO2` |
| Debug UART TX/RX | `GPIO0` / `GPIO1` |

### Eye Open Items

- Confirm the exact `LH128` display controller and initialization sequence.
- Decide whether left/right orientation is handled by firmware transform, display mounting, or separate configuration.
- Decide exact I2C addresses for left and right eye.

## Mouth Firmware Plan

The mouth RP2040 firmware renders animations on the HUB75-style matrix connector through 74AHCT244 level shifters.

### Mouth Firmware Responsibilities

| Area | Requirement |
|---|---|
| I2C slave | Listen on configured mouth address and parse display protocol |
| Matrix driver | Generate RGB data, row addressing, `CLK`, `LAT`, and `OE` timing |
| Animation engine | Render mouth expressions and speaking motion locally |
| Brightness | Use PWM/bitplane timing and `OE` to control brightness |
| Status LED | Use `GPIO2` for heartbeat/error/debug |
| Watchdog | Recover if matrix scan or I2C handling stalls |
| Debug | UART and SWD support through `J3` |

### Mouth Pin Use

| Matrix signal | RP2040 GPIO | Notes |
|---|---|---|
| `R1` | `GPIO6` | Upper red data |
| `G1` | `GPIO7` | Upper green data |
| `B1` | `GPIO8` | Upper blue data |
| `R2` | `GPIO9` | Lower red data |
| `G2` | `GPIO10` | Lower green data |
| `B2` | `GPIO11` | Lower blue data |
| `E` | `GPIO12` | Row address E |
| `A` | `GPIO13` | Row address A |
| `B` | `GPIO14` | Row address B |
| `C` | `GPIO15` | Row address C |
| `D` | `GPIO16` | Row address D |
| `CLK` | `GPIO17` | Pixel clock |
| `LAT` | `GPIO18` | Latch/strobe |
| `OE` | `GPIO19` | Output enable/brightness |

### Mouth Open Items

- Confirm matrix resolution, scan ratio, color depth, and row-address use of `E`.
- Decide the first set of mouth expression assets.
- Decide whether speaking motion is amplitude-only or viseme-based.

## Brain Audio Plan

### Output

The brain drives `U4` MAX98357A over I2S:

| Signal | ESP32 pin |
|---|---|
| `BCLK` | `IO39` |
| `LRCLK` | `IO40` |
| `DIN` | `IO41` |

ESP-IDF should use the I2S standard TX driver. 

### Input

The brain reads `MK1` ICS-43434 digital microphone over I2S:

| Signal | ESP32 pin |
|---|---|
| `BCLK` | `IO39` |
| `LRCLK` | `IO40` |
| `MicDATA` | `IO47` |

Because the speaker amp and microphone share `BCLK`/`LRCLK`, firmware should configure I2S carefully. If simultaneous full-duplex operation is needed, validate ESP-IDF I2S channel support and timing with this shared-clock topology.

## Brain Networking and Web Plan

### Networking

The brain should support at least one network path:

| Interface | Hardware | Firmware role |
|---|---|---|
| Wi-Fi | ESP32-S3 radio | Primary embedded web/API and internet access |
| Ethernet | W5500 over SPI | Wired network option |

W5500 pins:

| Signal | ESP32 pin |
|---|---|
| `EthCS` | `IO10` |
| `MOSI` | `IO11` |
| `SCLK` | `IO12` |
| `MISO` | `IO13` |
| `INTn` | `IO14` |
| `RSTn` | `IO15` |

### Embedded Web/API

The ESP32 should expose a compact web/API surface inspired by `SarcasmOS-web`:

| Endpoint | Method | Purpose |
|---|---|---|
| `/api/status` | `GET` | Return brain status, battery, network, display slave status |
| `/api/command` | `POST` | Trigger animation/expression/test commands |
| `/api/chat/text` | `POST` | Send text to assistant pipeline if implemented on-device/proxy |
| `/api/chat/audio` | `POST` | Upload/capture audio for assistant pipeline if supported |
| `/api/audio/{id}` | `GET` | Serve generated/cached audio if stored locally |
| `/api/config` | `GET/POST` | Read/update Wi-Fi, device addresses, preferences |

For the first embedded milestone, implement `/api/status` and `/api/command` only. The full AI pipeline can remain external until storage, RAM, and network behavior are validated on the ESP32-S3.

## Assistant State Machine

The firmware should centralize assistant behavior as explicit states:

| State | Entry action | Display command | Exit condition |
|---|---|---|---|
| `booting` | Initialize hardware | `thinking` or boot animation | Init complete |
| `idle` | Wait for input | `idle` | Button/API/wake/audio event |
| `listening` | Capture microphone/audio | `listening` | Speech end or timeout |
| `thinking` | Process STT/LLM/TTS or wait for backend | `thinking` | Response ready or timeout |
| `speaking` | Play audio | `speaking` | Audio playback complete |
| `error` | Log/report failure | `error` | Recovery timer/API reset |
| `sleep` | Dim displays | `sleep` | Wake event |

The state machine is the owner of display animation intent. Individual modules should request state transitions instead of directly commanding displays.

## Bring-Up Milestones

### Milestone 1: Electrical/Board Bring-Up

| Task | Success criteria |
|---|---|
| Brain boots ESP-IDF | Serial logs over UART0/USB |
| Enable 5 V rails | `+5V` and `5VHP` switch as expected |
| I2C scan | Charger, fuel gauge, eyes, and mouth detected |
| RP2040 slaves boot | Status LED heartbeat on each display board |
| Debug access | SWD/UART works on eye and mouth |

### Milestone 2: Display Slave Firmware

| Task | Success criteria |
|---|---|
| Eye display init | Each eye shows test pattern |
| Mouth matrix init | Mouth shows test pattern |
| I2C command parser | `PING`, `GET_INFO`, `SET_BRIGHTNESS` work |
| Animation command | `SET_ANIMATION(idle/thinking/speaking)` works |
| Unique addresses | Left eye, right eye, and mouth respond independently |

### Milestone 3: Brain Peripheral Firmware

| Task | Success criteria |
|---|---|
| W5500 or Wi-Fi networking | Brain reachable over network |
| HTTP API | `/api/status` and `/api/command` work |
| Audio out | Test sound plays through MAX98357A/speaker |
| Microphone input | Captured PCM shows valid signal |
| Power telemetry | Charger/fuel gauge status visible in API |

### Milestone 4: Integrated Assistant Behavior

| Task | Success criteria |
|---|---|
| State machine | Idle/listening/thinking/speaking transitions are stable |
| Display coordination | Three displays change animations together |
| Audio/display sync | Mouth speaking animation starts/stops with audio |
| Web command control | Web/API can trigger expressions and test animations |
| Failure handling | Missing slave/network/audio errors are reported cleanly |

## Configuration Data

Store these settings in brain NVS:

| Key | Purpose |
|---|---|
| `wifi.ssid`, `wifi.password` | Wi-Fi station configuration |
| `net.mode` | Wi-Fi, Ethernet, or both |
| `i2c.left_eye_addr` | Left eye I2C address |
| `i2c.right_eye_addr` | Right eye I2C address |
| `i2c.mouth_addr` | Mouth I2C address |
| `display.brightness` | Global brightness |
| `audio.volume` | Speaker volume/scaling |
| `assistant.backend_url` | Optional external AI/backend URL |

Store these settings in each RP2040 display board flash/config:

| Key | Applies to | Purpose |
|---|---|---|
| `device.role` | eyes/mouth | `left_eye`, `right_eye`, or `mouth` |
| `i2c.address` | eyes/mouth | Slave address |
| `display.orientation` | eyes | Left/right orientation/mirroring |
| `display.brightness_limit` | eyes/mouth | Maximum safe brightness |

## Open Decisions

These must be decided before final firmware implementation:

- Exact I2C addresses for left eye, right eye, and mouth. - up to firmware development decision, not important 
- How RP2040 board identity is provisioned. - different I2C addresses in firmware config
- Exact `LH128` display controller and initialization sequence. (
        Use SPI at 24–62.5 MHz depending on wiring quality.
Full framebuffer at RGB565 is:
240 × 240 × 2 = 115,200 bytes, which fits easily in RP2040 RAM.
The display is round, but the controller still uses a 240×240 square address space. You must clip/mask corners in software if needed.
Color format is normally RGB565.
)


- Mouth matrix resolution, scan rate, and color depth. -  (
Mouth display:
- Resolution: 128 × 64 RGB pixels
- Interface: HUB75/HUB75E
- Scan rate: 1/32 scan
- Address lines: A, B, C, D, E
- Pixel format: RGB
- Recommended framebuffer: RGB565
- Recommended output/PWM depth: 4 bits per color channel minimum
- Panel power: 5 V, up to 4 A per module)

- Whether the AI pipeline runs on the ESP32, proxies to a local server, or calls cloud APIs directly.
- Whether Wi-Fi, W5500 Ethernet, or both are required for the first release. wifi, is mandatory, ethernet able to be used, but disabled by default
- Whether speech-mouth sync is amplitude-based or viseme/phoneme-based. - whatever is easier to implement
