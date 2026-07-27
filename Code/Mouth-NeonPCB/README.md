# SarcasmOS ESP32-S3 mouth

Arduino C++ firmware for the custom ESP32-S3-MINI-1-N8 driver PCB and one
64x32, 1/16-scan HUB75 RGB panel. It renders the same mouth states as
`../Mouth`, but receives commands wirelessly over ESP-NOW instead of I2C.

Supported commands are ping, device info, brightness, animation/expression,
animation phase sync, stop, speaking intensity, and reset. Every valid command
gets a status response carrying its sequence number and result.

## Build, upload, and identify the board

```sh
pio run
pio run --target upload
pio device monitor
```

At boot the serial monitor prints the mouth's Wi-Fi station MAC address and
ESP-NOW channel. Record that MAC for the brain configuration.

The channel defaults to `1`. Change `-DESPNOW_CHANNEL=1` in `platformio.ini`
if the brain uses another 2.4 GHz channel. Both devices must use the same
channel.

## Display behavior

The firmware starts in the orange idle state and supports the animation IDs
already used by SarcasmOS:

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

Brightness defaults to `64/255`. `SET_PARAM` key `1` controls speaking mouth
intensity.

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
