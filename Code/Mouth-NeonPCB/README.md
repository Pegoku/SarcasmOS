# SarcasmOS ESP32-S3 mouth

Arduino C++ firmware for the custom ESP32-S3-MINI-1-N8 driver PCB and one
64x32, 1/16-scan HUB75 RGB panel. It renders the same mouth states as
`../Mouth`, but receives commands wirelessly over ESP-NOW instead of I2C.

The complete Workflow-derived visual and state contract is documented in
[`MOUTH_STATES.md`](MOUTH_STATES.md).

Supported commands are ping, device info, brightness, animation/expression,
animation phase sync, stop, speaking intensity, and reset. Every valid command
gets a status response carrying its sequence number and result.

## Firmware variants

The project produces two separate firmware images that share the same panel
driver, pin map, and animation renderer:

- `custom_esp32s3_mini_n8`: regular firmware with ESP-NOW control.
- `local_animation_test`: standalone tester with no brain or wireless link
  required.

### Regular ESP-NOW firmware

```sh
pio run -e custom_esp32s3_mini_n8
pio run -e custom_esp32s3_mini_n8 --target upload
```

At boot the serial monitor prints the mouth's Wi-Fi station MAC address and
ESP-NOW channel. Record that MAC for the brain configuration.

The channel defaults to `1`. Change `-DESPNOW_CHANNEL=1` in `platformio.ini`
if the brain uses another 2.4 GHz channel. Both devices must use the same
channel.

### Local animation-test firmware

```sh
pio run -e local_animation_test
pio run -e local_animation_test --target upload
pio device monitor -e local_animation_test
```

The local tester cycles through every supported state so visual changes can
be checked without the Brain. Its serial controls are:

| Key | Action |
| --- | --- |
| `0`..`9` | select one animation |
| Left arrow | select the previous state and pause autoplay |
| Right arrow | select the next state and pause autoplay |
| `a` | toggle automatic all-state cycling |
| `n` | advance to the next state |
| `p` | cycle red, green, blue, white, color bars, RGB rows, and geometry tests |
| `+` / `-` | adjust brightness |
| `]` / `[` | adjust speaking intensity |
| `h` | print help |

Uploading one variant replaces the other in flash. Re-upload the regular
environment when local testing is complete.

## Desktop display emulator

The dependency-free desktop emulator opens a scaled window representing the
physical 64x32 panel:

```sh
./emulator.py
```

The emulator and firmware consume the same source-of-truth asset pack:
[`assets/mouth_assets.json`](assets/mouth_assets.json). It contains every
native 64x32 sprite, frame sequence, playback mode, and frame duration.
PlatformIO regenerates `generated/mouth_assets.hpp` from that file before
building the firmware.

Left and Right select states and pause autoplay. Space or `a` resumes it;
`+`/`-` changes brightness, `[`/`]` changes speaking intensity, and `q` exits.

The editor controls operate on the exact animation frame currently displayed:

- `e` or **Open frame in GIMP** exports the native 64x32 sprite and opens it.
  Save/overwrite the PPM in GIMP, then return to the emulator.
- `i` or **Import saved edit** quantizes the saved image to the shared palette,
  updates `mouth_assets.json`, regenerates the firmware header, and reloads
  the window.
- `c` or **Copy frame** puts a native-resolution PNG on the Wayland clipboard.
- `r` or **Reload assets** reloads changes made outside the emulator.

Start paused on a particular state with:

```sh
./emulator.py --state sarcastic --paused
```

The renderer can also be validated on a machine without a graphical session:

```sh
./emulator.py --self-test
./emulator.py --state speaking --dump speaking.ppm
```

Sprites can also be managed without opening the GUI:

```sh
./asset_tool.py export angry angry.ppm
./asset_tool.py import angry angry.ppm
./asset_tool.py validate
./asset_tool.py compile
```

See [`assets/README.md`](assets/README.md) for the asset format and editing
workflow. Do not edit `generated/mouth_assets.hpp` manually.

ESP-IDF QEMU has an optional virtual RGB framebuffer, but the Arduino HUB75
DMA driver used here does not target that virtual peripheral. The desktop
emulator avoids a second ESP-IDF-specific display driver and starts without an
ESP toolchain.

## Display behavior

The firmware starts with Bender's pale-yellow tooth grille. IDs `0x00` through
`0x09` preserve the legacy SarcasmOS assignments; the remaining IDs cover the
complete Workflow state set:

| ID | State |
| ---: | --- |
| `0x00` | idle |
| `0x01` | listening |
| `0x02` | thinking |
| `0x03` | thinking with audio |
| `0x04` | long thinking |
| `0x05` | speaking |
| `0x06` | happy |
| `0x07` | angry |
| `0x08` | error |
| `0x09` | sleep |
| `0x0a` | tool |
| `0x0b` | left |
| `0x0c` | right |
| `0x0d` | up |
| `0x0e` | down |
| `0x0f` | center |
| `0x10` | neutral |
| `0x11` | sarcastic |
| `0x12` | suspicious |
| `0x13` | tired |
| `0x14` | surprised |
| `0x15` | bored |
| `0x16` | dramatic |
| `0x17` | watch |
| `0x18` | party |
| `0x19` | battery low |
| `0x1a` | sunny |
| `0x1b` | rainy |
| `0x1c` | cloudy |
| `0x1d` | stormy |
| `0x1e` | snowy |

Brightness defaults to `64/255`. `SET_PARAM` key `1` controls speaking mouth
intensity. Gaze-only states deliberately retain the neutral resting mouth,
because gaze affects the eyes rather than replacing the mouth expression.
Detailed behavior is described in `MOUTH_STATES.md`.

## ESP-NOW packet format

All multi-byte payload values are little-endian. Packets are variable length:

| Offset | Size | Meaning |
| ---: | ---: | --- |
| 0 | 2 | ASCII magic `SM` |
| 2 | 1 | transport version (`1`) |
| 3 | 1 | type: command `1`, status `2` |
| 4 | 1 | role: mouth `2`, or any role `255` |
| 5 | 1 | sequence |
| 6 | 1 | command |
| 7 | 1 | payload length, `0..64` |
| 8 | N | payload |
| 8+N | 1 | CRC-8/ATM over all previous bytes |

CRC uses polynomial `0x07`, initial value `0`, no reflection, and no final
XOR. Command IDs and packet helpers are in `protocol.hpp`.

The status payload is:

| Offset | Meaning |
| ---: | --- |
| 0 | application protocol version |
| 1 | role (`2`) |
| 2..3 | firmware major/minor |
| 4 | current animation |
| 5 | last accepted sequence |
| 6 | result: `0` OK, `1` malformed, `2` unknown command, `3` bad payload |
| 7 | brightness |
| 8 | speaking intensity |
| 9 | ESP-NOW channel |

The mouth learns the sender MAC from an incoming broadcast or unicast command,
adds it as an unencrypted peer, and returns status by unicast. A repeated
sequence from the same sender is acknowledged again without executing the
command twice.

## Driver PCB pin map

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
unexpected boot failures warrant checking GPIO3 during reset.

## Hardware checks

- The PCB, both SN74AHCT245 level shifters, and panel require 5 V.
- Use the matrix's separate high-current 5 V power connector.
- PCB ground, panel ground, and power-supply ground must be common.
- Connect the PCB to the matrix HUB75 input, not its output.
- This firmware assumes a conventional 64x32, 1/16-scan panel.

Do not power the LED matrix from the ESP32-S3's 3.3 V rail.
