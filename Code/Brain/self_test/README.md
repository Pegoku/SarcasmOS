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
- expected eye and mouth controller addresses (`0x30` through `0x32`)
- W5500 SPI communication through its version register
- an active Wi-Fi network scan (no credentials are required)
- I2S microphone samples and an optional short 440 Hz speaker tone

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
