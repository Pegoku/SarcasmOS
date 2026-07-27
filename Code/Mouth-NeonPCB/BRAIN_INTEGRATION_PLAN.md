# Brain integration plan: ESP-NOW mouth

This plan migrates only the mouth link to ESP-NOW. The two eyes and the
MAX17049/BQ25792 devices can remain on the brain's I2C bus.

## Target architecture

```text
HTTP/state logic
      |
      +-- left eye  ---- I2C 0x30
      +-- right eye ---- I2C 0x31
      `-- mouth -------- ESP-NOW unicast + status acknowledgement
```

The brain remains the controller. It sends the same command IDs and animation
IDs it already sends to `0x32`, wrapped in the ESP-NOW packet documented in
`README.md`. The mouth returns a status packet for every valid request.

## 1. Record the mouth identity and channel

1. Flash the mouth and open its 115200-baud serial monitor.
2. Record the station MAC printed as `ESP-NOW mouth MAC ...`.
3. Decide which 2.4 GHz channel the system will use.
   - If the brain joins a Wi-Fi access point, the access point determines the
     brain's channel. Read it with `esp_wifi_get_channel()` after connection.
   - Set the mouth's `ESPNOW_CHANNEL` build flag to that channel.
   - If the brain does not join an access point, configure both boards with a
     fixed channel such as channel 1.

Do not call `esp_wifi_set_channel()` on the brain after it has joined an access
point. That would break the infrastructure Wi-Fi connection.

## 2. Share the protocol definition

Move or copy `Mouth-NeonPCB/protocol.hpp` into a small shared protocol
component that can be consumed by Arduino C++ and ESP-IDF C/C++. If the brain
continues to compile `main.c` as C, create an equivalent `display_protocol.h`
with constants plus C encode/decode functions.

Add a host-side unit test with these checks:

- encode then decode every supported command;
- reject bad magic, version, length, CRC, type, and role;
- verify the CRC-8/ATM check against fixed byte vectors;
- verify maximum payload length 64 and reject 65.

Keeping one protocol definition avoids the command constants drifting between
the boards.

## 3. Add brain configuration

Add these options to `Brain/main/Kconfig.projbuild`:

```text
SARCASMOS_MOUTH_ESPNOW
SARCASMOS_MOUTH_MAC             # "AA:BB:CC:DD:EE:FF"
SARCASMOS_ESPNOW_CHANNEL        # used when not associated with an AP
SARCASMOS_ESPNOW_ACK_TIMEOUT_MS # default 100
SARCASMOS_ESPNOW_RETRIES        # default 3
```

Parse the configured MAC once during startup. Treat an invalid, multicast, or
all-zero MAC as a configuration error and report the mouth as unavailable.
Do not commit a personal board MAC if the repository is intended to build for
multiple robots; put it in `sdkconfig` or provision it through NVS instead.

## 4. Initialize the brain radio and ESP-NOW

Refactor `Brain/main/main.c` so Wi-Fi radio initialization happens even when
no SSID is configured:

1. Initialize NVS, netif, the default event loop, and Wi-Fi.
2. Set `WIFI_MODE_STA` and start Wi-Fi.
3. If an SSID is configured, connect and wait for `IP_EVENT_STA_GOT_IP`.
4. Otherwise set `CONFIG_SARCASMOS_ESPNOW_CHANNEL` with
   `esp_wifi_set_channel()`.
5. Call `esp_now_init()`.
6. Register receive and send callbacks.
7. Add the mouth MAC as an unencrypted peer on `WIFI_IF_STA`. Use channel `0`
   for the peer when the brain is associated with an AP, meaning the current
   radio channel; otherwise use the configured fixed channel.

Initialization order matters: ESP-NOW uses the Wi-Fi radio, so initialize it
after Wi-Fi has started and before the first display command is sent.

## 5. Add a small asynchronous transport

Create a dedicated mouth transport module rather than sending directly from
the ESP-NOW callback:

```text
mouth_espnow_init()
mouth_espnow_send(command, payload, length, wait_for_ack)
mouth_espnow_is_present()
mouth_espnow_get_status()
```

The receive callback runs in the Wi-Fi task. It should only validate the source
MAC and copy the packet into a FreeRTOS queue. A mouth task should decode it,
validate the CRC/type/role, and update status under a mutex.

For each new command:

1. Allocate the next nonzero 8-bit sequence number.
2. Encode a command packet with role `2`.
3. Send it to the configured mouth MAC.
4. Wait up to 100 ms for a status packet with the same source MAC, sequence,
   and echoed command.
5. If no matching status arrives, retry up to three times using the **same**
   sequence.
6. Mark the mouth present only after a valid status response.

The ESP-NOW send callback only confirms delivery at the radio/MAC layer. Use
the mouth's status response as the application-level acknowledgement.
Reusing the sequence on retries is safe because the mouth acknowledges
duplicates without executing them twice.

Serialize commands with a mutex or a single command queue so only one
acknowledged command is outstanding. This makes an 8-bit sequence sufficient
and prevents responses from being matched to the wrong waiter.

## 6. Route mouth commands away from I2C

In the current brain firmware:

1. Remove the mouth entry at I2C address `0x32` from `g_displays`, or mark its
   transport as ESP-NOW.
2. Keep `i2c_send()` for the left and right eyes.
3. Replace `display_command_all()` with transport-aware routing:
   - send to `0x30` and `0x31` over I2C;
   - send the same command/payload to the mouth over ESP-NOW.
4. Keep animation and sync command generation in one place so all three
   displays receive the same state change.

Suggested routing interface:

```c
esp_err_t display_command(display_role_t role, uint8_t command,
                          const uint8_t *payload, uint8_t length);
esp_err_t display_command_all(uint8_t command,
                              const uint8_t *payload, uint8_t length);
```

The existing command paths then change as follows:

| Brain operation | Mouth command/payload |
| --- | --- |
| startup brightness | `0x10`, one brightness byte |
| assistant state | `0x20`, one animation byte |
| phase synchronization | `0x22`, four-byte little-endian phase |
| speaking level | `0x30`, `{1, intensity}` |
| weather temperature | `0x30`, `{2, signed_int8_celsius}` |
| sleep/stop | `0x23`, no payload |
| health poll | `0x01`, no payload |

Send brightness before the first animation. During speaking, rate-limit
intensity updates to about 20-25 Hz and drop stale queued intensity commands;
mouth movement should not build up seconds of latency.

For weather results, send the real temperature parameter before selecting
`sunny`, `rainy`, `cloudy`, `stormy`, or `snowy`. Encode negative values in
two's-complement signed int8 form (`-5` is `0xFB`, `-10` is `0xF6`). Use
`-128` (`0x80`) to clear unavailable or stale temperature data. Do not select
a weather state using fabricated data merely to exercise the display; the
emulator and local test firmware already provide explicit test data.

## 7. Expose wireless mouth health

Extend the brain's mouth device state with:

```text
MAC
present
last sequence/result
firmware version
current animation
brightness
speaking intensity
ESP-NOW channel
last acknowledged time
retry/timeout counters
```

Update `/api/status` to expose those values. Poll with `PING` every three
seconds, matching the current I2C health cadence. Mark the mouth offline after
three consecutive unanswered polls, but do not block eye animation updates
while the mouth is unavailable.

## 8. Add discovery only if fixed provisioning is undesirable

The simplest and most deterministic production setup is a provisioned mouth
MAC. Optional discovery can be added later:

1. Add the broadcast peer `FF:FF:FF:FF:FF:FF`.
2. Broadcast a role-2 `PING`.
3. Accept only a valid role-2 status response.
4. Store the discovered unicast MAC in NVS.
5. Require physical pairing mode or a short pairing window to prevent binding
   to a nearby robot.

Do not use broadcast for normal commands because it has no link-layer
acknowledgement and makes multi-robot installations ambiguous.

## 9. Security hardening

The first integration can use the firmware's current unencrypted ESP-NOW
peer. Before deploying near untrusted devices:

1. Provision a common PMK on the brain.
2. Provision a unique 16-byte LMK for this brain/mouth pair.
3. Set `peer.encrypt = true` on both boards.
4. Store keys in NVS rather than source control.
5. Add a monotonically increasing session counter or nonce if replayed facial
   commands are a concern.

Keep CRC validation even with encryption; it still detects malformed
application packets and protocol mistakes.

## 10. Integration and acceptance tests

Run these in order:

1. **Radio bring-up:** both serial logs show the intended channel; brain can
   ping the mouth and parse firmware `3.0`, application protocol `2`, role `2`.
2. **Command coverage:** verify all animation IDs `0x00` through `0x1e`,
   brightness `0/64/255`, intensity `0/120/255`, stop, and reset.
3. **Acknowledgement:** verify returned source MAC, sequence, echoed command,
   role, CRC, and result for every request.
4. **Retry:** temporarily shield or power-cycle the mouth; confirm retries use
   one sequence and a recovered command executes once.
5. **Coexistence:** while the brain is associated with its access point,
   exercise HTTP traffic and continuous speaking animation for ten minutes.
6. **Failure isolation:** power off the mouth and verify eye I2C commands,
   audio, HTTP, and the brain task watchdog continue normally.
7. **Range/load:** test at the intended installed distance with the HUB75
   panel at maximum expected current and brightness.
8. **Reboot order:** test brain-first, mouth-first, and simultaneous boot.
9. **Multi-robot:** if applicable, operate two pairs nearby and verify each
   brain accepts status only from its configured mouth MAC.

Integration is complete when state-to-animation latency is consistently under
100 ms at the installed range, retries recover dropped packets without
duplicate state changes, and the brain reports mouth loss/recovery without
affecting its wired devices.
