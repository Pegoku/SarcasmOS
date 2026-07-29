# Brain PCB self-test firmware

This is a separate ESP-IDF application for bring-up and manufacturing checks of
the SarcasmOS brain PCB. It does not replace the production firmware in the
parent directory.

The report is printed through the ESP32-S3 native USB Serial/JTAG connection. It
tests:

- ESP32-S3 identity, flash, MAC address, and native USB serial output
- status LED and the charger/interrupt GPIO states
- `+5V` and `5VHP` regulator enable controls
- an I2C bus scan, including BQ25792 and MAX17049 detection
- MAX17049 version, cell voltage, and state-of-charge reads
- expected wired eye controller addresses (`0x30` and `0x31`)
- acknowledged ESP-NOW communication with the configured wireless mouth
- W5500 SPI communication through its version register
- an active Wi-Fi network scan (no credentials are required)
- I2S microphone samples and an optional short 440 Hz speaker tone

After the automatic checks, a menu-driven terminal interface remains available
on the same USB serial connection. It can:

- explicitly enable or disable the `+5V` and `5VHP` buck converters
- enable or disable charging through the BQ25792 active-low `CE` input
- scan for Wi-Fi networks, select one, enter its password, and connect
- report the connected SSID, channel, RSSI, IP address, and disconnect reason
- disconnect Wi-Fi
- run separate one-second speaker-tone and microphone-capture tests
- address the I2C left eye, I2C right eye, or ESP-NOW mouth and visually cycle it
  through happy, error, and idle animations while checking its protocol status
- discover the two eyes by I2C protocol/role and the mouth by acknowledged
  ESP-NOW, select any combination with checkboxes, and repeatedly send any of
  the shared face states `0x00` through `0x1E`
- repeat the I2C, W5500, combined audio, and GPIO tests independently
- rerun the complete automatic test sequence

Wi-Fi credentials entered in the menu are kept in RAM only and are not saved to
flash. Password entry may be visible in the serial terminal.

The buck converters do not have voltage-sense or power-good signals connected
to the ESP32-S3. Their automatic result is therefore `WARN`: the firmware can
verify that the enable GPIO latched high, but a multimeter is required to verify
the actual output voltage.

The TMC2209 is deliberately left disabled so running the test cannot move a
connected motor.

## Build and flash

From `Code/Brain` with ESP-IDF loaded in the shell:

```sh
idf.py -C self_test set-target esp32s3
idf.py -C self_test build
idf.py -C self_test flash monitor
```

The USB monitor normally appears as `/dev/ttyACM0`. If automatic port detection
does not select it, add `-p /dev/ttyACM0` to the flash and monitor commands.

Optional tests and the audible tone can be changed with:

```sh
idf.py -C self_test menuconfig
```

Look under `SarcasmOS Brain self-test`.

Enter the station MAC printed by the mouth firmware in
`ESP-NOW mouth station MAC`. When the tester is not connected to an access
point, its configured ESP-NOW channel must match the mouth firmware's
`ESPNOW_CHANNEL`. An empty MAC safely makes the automatic wireless mouth result
`SKIP`.

## Manual terminal interface

When the initial report finishes, enter `h` to display the controls:

```text
brain-test> h
```

Input is echoed character by character as you type. Commands are single letters
or numbers. For example, enter `2` to disable the normal 5 V buck, `1` to enable
it again, `c` to select and connect to a Wi-Fi network, and `s` to show its
connection status and IP address. Use `p` for the speaker, `m` for the
microphone, `l`/`r` for the wired eyes, `o` for the wireless mouth, and `v` to
test all three displays. The complete automatic sequence is `x`.

Enter `f` for the interactive face display controller. It probes the left eye
at `0x30` and right eye at `0x31`, validates each eye's protocol role, and
requires an acknowledged ESP-NOW ping from the configured mouth. In the target
screen, move with the arrow keys or `W`/`S`, toggle checkboxes with Space, and
press Enter to continue. Unavailable displays cannot be selected; use `R` to
repeat discovery.

The state screen lists all shared Eye/Mouth animation IDs with descriptions,
ten at a time. Move with the arrow keys or `W`/`S` and press Enter (or Space)
to send the highlighted state to every selected target. The screen reports an
individual acknowledgment result for each display and remains open for
additional states. Press `Q` or Escape to return to the main test menu.

Enabling a buck only verifies the enable GPIO state. Measure the corresponding
rail before treating it as electrically validated.
