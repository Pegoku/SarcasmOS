# SarcasmOS PCB Schematics and Firmware Interface

This document summarizes the three KiCad PCB designs in this repository from a firmware point of view. It is intended for someone who does not have the PCB files open, but needs to understand what is connected to what in order to write firmware. The planned software architecture is documented separately in `FIRMWARE_PLAN.md`.

Source schematics checked:

- `PCB/brain/brain.kicad_sch`
- `PCB/eye/eye.kicad_sch`
- `PCB/mouth/mouth.kicad_sch`

## System Overview

The physical PCB set is split into three board designs, with four boards expected in the assembled robot:

- `brain`: Main controller board. It contains an ESP32-S3 module, USB-C/power management, battery charger/fuel gauge, Ethernet controller, audio amplifier/microphone interface, stepper motor driver, power regulators, and connectors to the other boards.
- `eye`: RP2040-based display board. The robot uses two copies of this PCB, one for each eye. Each eye drives an `LH128` 12-pin display module and exposes I2C, UART/SWD/debug, and display breakout connectors.
- `mouth`: RP2040-based HUB75-style LED matrix interface board. It drives a 16-pin RGB matrix connector through 74AHCT244 level shifters and exposes I2C plus UART/SWD/debug.

The expected firmware architecture is:

- ESP32-S3 on `brain` is the main controller.
- Two RP2040 eye boards are display coprocessors, reachable from the brain over I2C with different slave addresses.
- RP2040 on `mouth` is a LED matrix coprocessor, reachable from the brain over I2C.
- All runtime display control commands from the brain to the two eyes and the mouth are expected to use I2C.
- The brain runs the high-level voice-assistant logic: web/API control, audio playback/capture, networking, animation selection, and I2C commands to the display coprocessors.
- Optional UART/SWD connectors on `eye` and `mouth` are for firmware loading/debugging, not the primary brain-to-board runtime link unless firmware chooses to use them externally.

## Inter-Board Connections

### Brain to Eye or Mouth I2C/Power Bus

The brain has four identical JST-XH 4-pin connectors: `J2`, `J4`, `J5`, and `J6`. Each connector carries the same bus:

| Brain connector pin | Net | Function |
|---:|---|---|
| 1 | `+5V` | 5 V power output from brain |
| 2 | `SDA` | I2C data |
| 3 | `SCL` | I2C clock |
| 4 | `GND` | Ground |

The two `eye` boards `J2` and the `mouth` board `J2` have the same pinout:

| Eye/Mouth `J2` pin | Net | Function |
|---:|---|---|
| 1 | `+5V` | 5 V input |
| 2 | `SDA` | I2C data to RP2040 GPIO4 |
| 3 | `SCL` | I2C clock to RP2040 GPIO5 |
| 4 | `GND` | Ground |

The schematic does not assign a specific brain connector to left eye, right eye, or mouth; `J2`, `J4`, `J5`, and `J6` are electrically interchangeable I2C/power ports. A typical assembly would use three of the four connectors: one for left eye, one for right eye, and one for mouth.

### Runtime I2C Devices

The firmware should treat the display boards as separate I2C slaves on the same bus:

| Device | Board design | Connector | Role | I2C address |
|---|---|---|---|---|
| Left eye | `eye` | `J2` | Eye display coprocessor | Firmware-assigned, unique |
| Right eye | `eye` | `J2` | Eye display coprocessor | Firmware-assigned, unique |
| Mouth | `mouth` | `J2` | Mouth/LED matrix coprocessor | Firmware-assigned, unique |

The schematic does not include address-selection hardware for the eye boards, so left/right identity must be handled in firmware, flash configuration, build configuration, or another external provisioning step.

### I2C Electrical Details

On the brain board, the I2C bus has pull-ups to `+3.3V`:

| Signal | ESP32-S3 pin | Pull-up |
|---|---|---|
| `SDA` | `IO8` | `R12`, 4.7 kOhm to `+3.3V` |
| `SCL` | `IO9` | `R13`, 4.7 kOhm to `+3.3V` |

The I2C bus also connects to the brain's power-management ICs:

| Device | SDA | SCL | Notes |
|---|---|---|---|
| `U9` BQ25792 charger | pin 15 | pin 14 | Charger control/status |
| `U10` MAX17049 fuel gauge | pin 8 | pin 7 | Battery monitoring |
| External boards | connector pin 2 | connector pin 3 | Two eyes and mouth RP2040 I2C slaves |

Because pull-ups are on the brain, the eye and mouth firmware should normally configure `SDA`/`SCL` as open-drain I2C pins and should not enable strong pull-ups unless required.

## Brain PCB

### Main ICs

| Ref | Part | Firmware relevance |
|---|---|---|
| `U1` | ESP32-S3-WROOM-1 | Main MCU |
| `U2` | TMC2209-LA | Stepper motor driver |
| `U3` | W5500 | SPI Ethernet controller |
| `U4` | MAX98357A | I2S DAC/class-D speaker amplifier |
| `MK1` | ICS-43434 | I2S digital microphone |
| `U9` | BQ25792 | I2C battery charger/power-path controller |
| `U10` | MAX17049G+T10 | I2C battery fuel gauge |
| `U11` | AP64501SP-13 | High-power 5 V regulator, ESP32 enabled |
| `U12` | AP63205WU | 5 V regulator, ESP32 enabled |
| `U14` | AP63203WU | 3.3 V regulator |

### ESP32-S3 Firmware Pin Map

| ESP32-S3 signal | Module pad | Net | Connected to | Firmware use |
|---|---:|---|---|---|
| `EN` | 3 | `EN` | Reset pushbutton `EN1`, RC reset circuit | ESP32 enable/reset |
| `IO0` | 27 | `BOOT` | Boot pushbutton `BOOT1`, 10 kOhm pull-up | Bootloader strap button |
| `IO1` | 39 | `5V_EN` | `U12` enable | Enable/disable 5 V regulator |
| `IO2` | 38 | `5VHP_EN` | `U11` enable | Enable/disable high-power 5 V rail |
| `IO4` | 4 | `RX1` | `J11` pin 2 | UART1 RX or general I/O |
| `IO5` | 5 | `TX1` | `J11` pin 3 | UART1 TX or general I/O |
| `IO6` | 6 | `CE` | `U9` BQ25792 pin 13 | Charger CE control |
| `IO7` | 7 | `BQINT` | `U9` BQ25792 pin 21, `TP6` | Charger interrupt input |
| `IO8` | 12 | `SDA` | I2C bus, `U9`, `U10`, `J2/J4/J5/J6` | I2C SDA |
| `IO9` | 17 | `SCL` | I2C bus, `U9`, `U10`, `J2/J4/J5/J6` | I2C SCL |
| `IO10` | 18 | `EthCS` | `U3` W5500 `SCS`, `J7` pin 5 | Ethernet SPI chip-select |
| `IO11` | 19 | `MOSI` | `U3` W5500 `MOSI`, `J7` pin 3 | Ethernet SPI MOSI |
| `IO12` | 20 | `SCLK` | `U3` W5500 `SCLK`, `J7` pin 4 | Ethernet SPI clock |
| `IO13` | 21 | `MISO` | `U3` W5500 `MISO`, `J8` pin 6 | Ethernet SPI MISO |
| `IO14` | 22 | `INTn` | `U3` W5500 interrupt, `J7` pin 6 | Ethernet interrupt input |
| `IO15` | 8 | `RSTn` | `U3` W5500 reset, `J8` pin 5 | Ethernet reset output |
| `IO16` | 9 | `STEP` | `U2` TMC2209 `STEP`, `J10` pin 5 | Stepper step pulse |
| `IO17` | 10 | `DIR` | `U2` TMC2209 `DIR`, `J10` pin 6 | Stepper direction |
| `IO18` | 11 | `TmcEN` | `U2` TMC2209 `~EN`, `J10` pin 10 | Stepper driver enable, active low |
| `IO21` | 23 | `UART` | `U2` TMC2209 `PDN_UART`, `J10` pin 7 | TMC2209 single-wire UART |
| `IO38` | 31 | `DIAG` | `U2` TMC2209 `DIAG`, `J10` pin 11 | Stepper diagnostic input |
| `IO39` | 32 | `BCLK` | `U4` MAX98357A `BCLK`, `MK1` clock, `J13` pin 4 | I2S bit clock |
| `IO40` | 33 | `LRCLK` | `U4` MAX98357A `LRCLK`, `MK1` WS, `J13` pin 2 | I2S word select |
| `IO41` | 34 | `DIN` | `U4` MAX98357A `DIN` | I2S speaker data out |
| `IO42` | 35 | `ALRT` | `U10` MAX17049 alert | Battery/fuel-gauge alert input |
| `IO47` | 24 | `MicDATA` | `MK1` ICS-43434 data, `J13` pin 3 | I2S microphone data in |
| `IO48` | 25 | `Net-(D1-A)` | Status LED `D1` anode | User/status LED output |
| `RXD0` | 36 | `RX0` | `J14` pin 2 | ESP32 UART0 RX |
| `TXD0` | 37 | `TX0` | `J14` pin 1 | ESP32 UART0 TX |
| `USB_D-` | 13 | `D-` | USB-C through 22 Ohm resistor/ESD | Native USB D- |
| `USB_D+` | 14 | `D+` | USB-C through 22 Ohm resistor/ESD | Native USB D+ |

Unconnected ESP32-S3 module pins in the schematic: `IO3`, `IO35`, `IO36`, `IO37`, `IO45`, and `IO46`.

### Brain Connectors

#### Power Input/Output

| Connector | Pin | Net | Description |
|---|---:|---|---|
| `J1` screw terminal | 1 | `5VHP` | High-power 5 V rail output/input node |
| `J1` screw terminal | 2 | `GND` | Ground |
| `J12` battery JST | 1 | `Net-(BT2--)` | Battery stack negative/protection node |
| `J12` battery JST | 2 | `Net-(BT1--)` | Battery midpoint / BT1 negative / BT2 positive |
| `J12` battery JST | 3 | `+BATT` | Battery positive |

`USB1` is a USB-C connector. `VBUS` goes through fuse `F1` to `VUSB`, then into the charger/power path. `D+` and `D-` connect to the ESP32-S3 native USB pins through 22 Ohm series resistors and ESD protection.

#### I2C/Power Expansion Connectors

`J2`, `J4`, `J5`, and `J6` are identical 4-pin JST-XH connectors:

| Pin | Net | Description |
|---:|---|---|
| 1 | `+5V` | 5 V power |
| 2 | `SDA` | I2C data |
| 3 | `SCL` | I2C clock |
| 4 | `GND` | Ground |

#### Ethernet Debug/Breakout Headers

`J7` exposes Ethernet SPI-side signals:

| `J7` pin | Net | Function |
|---:|---|---|
| 1 | `GND` | Ground |
| 2 | `GND` | Ground |
| 3 | `MOSI` | SPI MOSI |
| 4 | `SCLK` | SPI clock |
| 5 | `EthCS` | W5500 chip select |
| 6 | `INTn` | W5500 interrupt |

`J8` exposes additional Ethernet-side/debug signals:

| `J8` pin | Net | Function |
|---:|---|---|
| 1 | `GND` | Ground |
| 2 | `+3.3V` | 3.3 V |
| 3 | `+3.3V` | 3.3 V |
| 4 | unconnected | No connection |
| 5 | `RSTn` | W5500 reset |
| 6 | `MISO` | SPI MISO |

#### Stepper Motor/TMC2209 Connectors

`J9` is the 4-pin motor coil connector:

| `J9` pin | Net | TMC2209 output |
|---:|---|---|
| 1 | `OA2` | Coil A output 2 |
| 2 | `OA1` | Coil A output 1 |
| 3 | `OB1` | Coil B output 1 |
| 4 | `OB2` | Coil B output 2 |

`J10` is a 12-pin TMC2209 control/power header:

| `J10` pin | Net | Description |
|---:|---|---|
| 1 | `VS` | Motor supply |
| 2 | `GND` | Ground |
| 3 | `GND` | Ground |
| 4 | `+3.3V` | Logic supply |
| 5 | `STEP` | Step input from ESP32 `IO16` |
| 6 | `DIR` | Direction input from ESP32 `IO17` |
| 7 | `UART` | TMC2209 UART from ESP32 `IO21` |
| 8 | `MS1` | Microstep/address pin |
| 9 | `MS2` | Microstep/address pin |
| 10 | `TmcEN` | Enable, active low, from ESP32 `IO18` |
| 11 | `DIAG` | Diagnostic output to ESP32 `IO38` |
| 12 | `INDEX` | TMC2209 index output |

### Brain Peripheral Details

#### W5500 Ethernet

The W5500 is connected to the ESP32-S3 by SPI:

| W5500 signal | Net | ESP32-S3 pin |
|---|---|---|
| `SCS` | `EthCS` | `IO10` |
| `SCLK` | `SCLK` | `IO12` |
| `MOSI` | `MOSI` | `IO11` |
| `MISO` | `MISO` | `IO13` |
| `INT` | `INTn` | `IO14` |
| `RST` | `RSTn` | `IO15` |

The W5500 connects to RJ45 MagJack `J3`. LED outputs are wired to RJ45 LED pins through resistors: `LINKLED` and `ACTLED`. The Ethernet PHY has a 25 MHz crystal.

#### Audio

The speaker amplifier is a MAX98357A on I2S:

| Signal | Net | ESP32-S3 pin | Also connected to |
|---|---|---|---|
| Bit clock | `BCLK` | `IO39` | MAX98357A pin 16, microphone pin 4, `J13` pin 4 |
| Word select | `LRCLK` | `IO40` | MAX98357A pin 14, microphone pin 1, `J13` pin 2 |
| Speaker data out | `DIN` | `IO41` | MAX98357A pin 1 |
| Microphone data in | `MicDATA` | `IO47` | ICS-43434 pin 6, `J13` pin 3 |

MAX98357A outputs go through ferrite beads to speaker `LS1`:

| MAX98357A pin | Net | Speaker side |
|---|---|---|
| `OUTP` pin 9 | `Net-(U4-OUTP)` | `LS1` pin 1 through `FB1` |
| `OUTN` pin 10 | `Net-(U4-OUTN)` | `LS1` pin 2 through `FB2` |

`J13` is a 5-pin audio/microphone header:

| `J13` pin | Net | Function |
|---:|---|---|
| 1 | `+3.3V` | 3.3 V power |
| 2 | `LRCLK` | I2S word select |
| 3 | `MicDATA` | I2S microphone data |
| 4 | `BCLK` | I2S bit clock |
| 5 | `GND` | Ground |

#### Power Management

The BQ25792 charger and MAX17049 fuel gauge share the brain I2C bus.

| Device signal | Net | ESP32-S3 pin | Firmware meaning |
|---|---|---|---|
| BQ25792 `SDA` | `SDA` | `IO8` | I2C data |
| BQ25792 `SCL` | `SCL` | `IO9` | I2C clock |
| BQ25792 `CE` | `CE` | `IO6` | Charger enable/control |
| BQ25792 `INT` | `BQINT` | `IO7` | Charger interrupt |
| MAX17049 `SDA` | `SDA` | `IO8` | I2C data |
| MAX17049 `SCL` | `SCL` | `IO9` | I2C clock |
| MAX17049 `ALRT` | `ALRT` | `IO42` | Fuel-gauge alert |

The ESP32 controls two 5 V regulators:

| Rail/control | Net | ESP32-S3 pin | Target IC |
|---|---|---|---|
| 5 V enable | `5V_EN` | `IO1` | `U12` AP63205WU enable |
| High-power 5 V enable | `5VHP_EN` | `IO2` | `U11` AP64501SP-13 enable |

## Eye PCB

### Main ICs

| Ref | Part | Firmware relevance |
|---|---|---|
| `U1` | RP2040 | Eye/display MCU |
| `U2` | W25Q32JVSS | External QSPI flash for RP2040 |
| `U3` | AP2112K-3.3 | 3.3 V regulator from 5 V input |
| `J1` | LH128 | Display connector/module |
| `Q1` | AO3400A | Backlight low-side switch |

### RP2040 Firmware Pin Map

| RP2040 GPIO/signal | Package pin | Net | Connected to | Firmware use |
|---|---:|---|---|---|
| `GPIO0` | 2 | `TX` | `J3` pin 7 | UART TX/debug |
| `GPIO1` | 3 | `RX` | `J3` pin 8 | UART RX/debug |
| `GPIO2` | 4 | `Net-(D1-A)` | LED `D1` anode | User/status LED |
| `GPIO4` | 6 | `SDA` | `J2` pin 2 | I2C SDA to brain |
| `GPIO5` | 7 | `SCL` | `J2` pin 3 | I2C SCL to brain |
| `GPIO6` | 8 | `Net-(U1-GPIO6)` | Test point `G6` | Test point / spare I/O |
| `GPIO7` | 9 | `Net-(U1-GPIO7)` | Test point `G7` | Test point / spare I/O |
| `GPIO8` | 11 | `Net-(U1-GPIO8)` | Test point `G8` | Test point / spare I/O |
| `GPIO9` | 12 | `Net-(U1-GPIO9)` | Test point `G9` | Test point / spare I/O |
| `GPIO10` | 13 | `Net-(U1-GPIO10)` | Test point `G10` | Test point / spare I/O |
| `GPIO11` | 14 | `Net-(U1-GPIO11)` | Test point `G11` | Test point / spare I/O |
| `GPIO12` | 15 | `Net-(U1-GPIO12)` | Test point `G12` | Test point / spare I/O |
| `GPIO13` | 16 | `Net-(U1-GPIO13)` | Test point `G13` | Test point / spare I/O |
| `GPIO14` | 17 | `Net-(U1-GPIO14)` | Test point `G14` | Test point / spare I/O |
| `GPIO15` | 18 | `Net-(U1-GPIO15)` | Test point `G15` | Test point / spare I/O |
| `GPIO17` | 28 | `CS` | Display `J1` pin 8, breakout `J4` pin 5 | Display chip select |
| `GPIO18` | 29 | `SCK` | Display `J1` pin 9, breakout `J4` pin 6 | Display SPI clock |
| `GPIO19` | 30 | `MOSI` | Display `J1` pin 10, breakout `J4` pin 7 | Display SPI data |
| `GPIO20` | 31 | `DC` | Display `J1` pin 7, breakout `J4` pin 4 | Display data/command |
| `GPIO21` | 32 | `RST` | Display `J1` pin 11, breakout `J4` pin 8 | Display reset |
| `GPIO22` | 34 | `BL_PIN` | Backlight MOSFET gate through 100 Ohm | Backlight PWM/on-off |
| `RUN` | 26 | `RUN` | `RUN1` button, `J3` pin 6 | RP2040 reset/run |
| `SWCLK` | 24 | `SWCLK` | `J3` pin 5 | SWD clock |
| `SWDIO` | 25 | `SWDIO` | `J3` pin 4 | SWD data |

The RP2040 QSPI pins are connected to external flash `U2` and are not general-purpose firmware pins:

| RP2040 pin | Net | Flash pin |
|---|---|---|
| `QSPI_SCLK` | `/QSPI_SCLK` | `U2` pin 6 |
| `QSPI_SD0` | `/QSPI_SD0` | `U2` pin 5 |
| `QSPI_SD1` | `/QSPI_SD1` | `U2` pin 2 |
| `QSPI_SD2` | `/QSPI_SD2` | `U2` pin 3 |
| `QSPI_SD3` | `/QSPI_SD3` | `U2` pin 7 |
| `QSPI_SS` | `/QSPI_SS` | `U2` pin 1 |

### Eye Connectors

#### Brain I2C/Power Connector `J2`

| Pin | Net | Function |
|---:|---|---|
| 1 | `+5V` | 5 V input from brain |
| 2 | `SDA` | I2C SDA to RP2040 `GPIO4` |
| 3 | `SCL` | I2C SCL to RP2040 `GPIO5` |
| 4 | `GND` | Ground |

#### Debug Header `J3`

| Pin | Net | Function |
|---:|---|---|
| 1 | `+5V` | 5 V |
| 2 | `Net-(J3-Pin_2)` | Connected through `JP1`/`R7`; optional 3.3 V related node |
| 3 | `GND` | Ground |
| 4 | `SWDIO` | SWD data |
| 5 | `SWCLK` | SWD clock |
| 6 | `RUN` | RP2040 run/reset |
| 7 | `TX` | RP2040 UART TX (`GPIO0`) |
| 8 | `RX` | RP2040 UART RX (`GPIO1`) |

#### Display Connector `J1` (`LH128`)

| Pin | Net | Function |
|---:|---|---|
| 1 | `GND` | Ground |
| 2 | `BL` | Backlight supply/switched return path |
| 3 | `+3.3V` | Display 3.3 V |
| 4 | `+3.3V` | Display 3.3 V |
| 5 | `GND` | Ground |
| 6 | `GND` | Ground |
| 7 | `DC` | Display data/command from RP2040 `GPIO20` |
| 8 | `CS` | Display chip-select from RP2040 `GPIO17` |
| 9 | `SCK` | Display SPI clock from RP2040 `GPIO18` |
| 10 | `MOSI` | Display SPI MOSI from RP2040 `GPIO19` |
| 11 | `RST` | Display reset from RP2040 `GPIO21` |
| 12 | `GND` | Ground |

#### Display Breakout Header `J4`

`J4` exposes the same display interface in an 8-pin header:

| Pin | Net | Function |
|---:|---|---|
| 1 | `+3.3V` | 3.3 V |
| 2 | `GND` | Ground |
| 3 | `BL` | Backlight |
| 4 | `DC` | Display data/command |
| 5 | `CS` | Display chip select |
| 6 | `SCK` | Display SPI clock |
| 7 | `MOSI` | Display SPI data |
| 8 | `RST` | Display reset |

### Eye Firmware Notes

- The display is SPI write-only from the RP2040 schematic point of view: `CS`, `SCK`, `MOSI`, `DC`, `RST`, and `BL_PIN` are controlled by RP2040 firmware.
- Backlight is driven by `GPIO22` through `R1` into MOSFET `Q1`; use PWM if brightness control is desired.
- `D1` is a firmware-controllable status LED on `GPIO2`.
- `D2` and `D3` are power indicator LEDs on `+5V` and `+3.3V` rails.
- USB D+/D- pins on RP2040 are unconnected; programming/debug should use SWD/UART unless a bootloader is already present in flash.

## Mouth PCB

### Main ICs

| Ref | Part | Firmware relevance |
|---|---|---|
| `U1` | RP2040 | Mouth/LED matrix MCU |
| `U2` | W25Q32JVSS | External QSPI flash for RP2040 |
| `U3` | AP2112K-3.3 | 3.3 V regulator from 5 V input |
| `U4`, `U5` | 74AHCT244 | 3.3 V to 5 V level shifting/buffering for matrix outputs |
| `J5` | 2x8 connector | HUB75-style RGB matrix output |

### RP2040 Firmware Pin Map

| RP2040 GPIO/signal | Package pin | Net | Output after level shift | Matrix signal |
|---|---:|---|---|---|
| `GPIO0` | 2 | `TX` | n/a | UART TX/debug on `J3` pin 7 |
| `GPIO1` | 3 | `RX` | n/a | UART RX/debug on `J3` pin 8 |
| `GPIO2` | 4 | `Net-(D1-A)` | n/a | User/status LED |
| `GPIO4` | 6 | `SDA` | n/a | I2C SDA to brain on `J2` pin 2 |
| `GPIO5` | 7 | `SCL` | n/a | I2C SCL to brain on `J2` pin 3 |
| `GPIO6` | 8 | `PIN_R1` | `R1` | Upper red data |
| `GPIO7` | 9 | `PIN_G1` | `G1` | Upper green data |
| `GPIO8` | 11 | `PIN_B1` | `B1` | Upper blue data |
| `GPIO9` | 12 | `PIN_R2` | `R2` | Lower red data |
| `GPIO10` | 13 | `PIN_G2` | `G2` | Lower green data |
| `GPIO11` | 14 | `PIN_B2` | `B2` | Lower blue data |
| `GPIO12` | 15 | `PIN_E` | `E` | Row address E |
| `GPIO13` | 16 | `PIN_A` | `A` | Row address A |
| `GPIO14` | 17 | `PIN_B` | `B` | Row address B |
| `GPIO15` | 18 | `PIN_C` | `C` | Row address C |
| `GPIO16` | 27 | `PIN_D` | `D` | Row address D |
| `GPIO17` | 28 | `PIN_CLK` | `CLK` | Matrix pixel clock |
| `GPIO18` | 29 | `PIN_LAT` | `LAT` | Matrix latch/strobe |
| `GPIO19` | 30 | `PIN_OE` | `OE` | Matrix output enable |
| `RUN` | 26 | `RUN` | n/a | RP2040 reset/run on `J3` pin 6 |
| `SWCLK` | 24 | `SWCLK` | n/a | SWD clock on `J3` pin 5 |
| `SWDIO` | 25 | `SWDIO` | n/a | SWD data on `J3` pin 4 |

The RP2040 QSPI pins are connected to external flash `U2` and are not general-purpose firmware pins:

| RP2040 pin | Net | Flash pin |
|---|---|---|
| `QSPI_SCLK` | `/QSPI_SCLK` | `U2` pin 6 |
| `QSPI_SD0` | `/QSPI_SD0` | `U2` pin 5 |
| `QSPI_SD1` | `/QSPI_SD1` | `U2` pin 2 |
| `QSPI_SD2` | `/QSPI_SD2` | `U2` pin 3 |
| `QSPI_SD3` | `/QSPI_SD3` | `U2` pin 7 |
| `QSPI_SS` | `/QSPI_SS` | `U2` pin 1 |

### Mouth Connectors

#### External 5 V Power `J1`

| Pin | Net | Function |
|---:|---|---|
| 1 | `+5V` | 5 V input for board/matrix logic |
| 2 | `GND` | Ground |

#### Brain I2C/Power Connector `J2`

| Pin | Net | Function |
|---:|---|---|
| 1 | `+5V` | 5 V input from brain |
| 2 | `SDA` | I2C SDA to RP2040 `GPIO4` |
| 3 | `SCL` | I2C SCL to RP2040 `GPIO5` |
| 4 | `GND` | Ground |

#### Debug Header `J3`

| Pin | Net | Function |
|---:|---|---|
| 1 | `+5V` | 5 V |
| 2 | `Net-(J3-Pin_2)` | Connected through `JP1`/`R7`; optional 3.3 V related node |
| 3 | `GND` | Ground |
| 4 | `SWDIO` | SWD data |
| 5 | `SWCLK` | SWD clock |
| 6 | `RUN` | RP2040 run/reset |
| 7 | `TX` | RP2040 UART TX (`GPIO0`) |
| 8 | `RX` | RP2040 UART RX (`GPIO1`) |

#### HUB75-Style Matrix Connector `J5`

| `J5` pin | Net | Source |
|---:|---|---|
| 1 | `R1` | RP2040 `GPIO6` through `U4` + 22 Ohm series resistor |
| 2 | `G1` | RP2040 `GPIO7` through `U4` + 22 Ohm series resistor |
| 3 | `B1` | RP2040 `GPIO8` through `U4` + 22 Ohm series resistor |
| 4 | `GND` | Ground |
| 5 | `R2` | RP2040 `GPIO9` through `U4` + 22 Ohm series resistor |
| 6 | `G2` | RP2040 `GPIO10` through `U4` + 22 Ohm series resistor |
| 7 | `B2` | RP2040 `GPIO11` through `U4` + 22 Ohm series resistor |
| 8 | `E` | RP2040 `GPIO12` through `U5` + 22 Ohm series resistor |
| 9 | `A` | RP2040 `GPIO13` through `U4` + 22 Ohm series resistor |
| 10 | `B` | RP2040 `GPIO14` through `U4` + 22 Ohm series resistor |
| 11 | `C` | RP2040 `GPIO15` through `U5` + 22 Ohm series resistor |
| 12 | `D` | RP2040 `GPIO16` through `U5` + 22 Ohm series resistor |
| 13 | `CLK` | RP2040 `GPIO17` through `U5` + 22 Ohm series resistor |
| 14 | `LAT` | RP2040 `GPIO18` through `U5` + 22 Ohm series resistor |
| 15 | `OE` | RP2040 `GPIO19` through `U5` + 22 Ohm series resistor; also 10 kOhm pull-up to `+5V` |
| 16 | `GND` | Ground |

### Mouth Level Shifters

The mouth PCB uses two `74AHCT244` buffers powered from `+5V`. Their inputs are driven directly by RP2040 3.3 V GPIOs; AHCT inputs accept 3.3 V CMOS levels and output 5 V logic to the matrix connector.

| RP2040 net | Buffer input | Buffer output net | Connector net |
|---|---|---|---|
| `PIN_R1` | `U4` pin 2 | `U4` pin 18 | `R1` |
| `PIN_G1` | `U4` pin 4 | `U4` pin 16 | `G1` |
| `PIN_B1` | `U4` pin 6 | `U4` pin 14 | `B1` |
| `PIN_R2` | `U4` pin 8 | `U4` pin 12 | `R2` |
| `PIN_G2` | `U4` pin 17 | `U4` pin 3 | `G2` |
| `PIN_B2` | `U4` pin 15 | `U4` pin 5 | `B2` |
| `PIN_A` | `U4` pin 13 | `U4` pin 7 | `A` |
| `PIN_B` | `U4` pin 11 | `U4` pin 9 | `B` |
| `PIN_C` | `U5` pin 2 | `U5` pin 18 | `C` |
| `PIN_D` | `U5` pin 4 | `U5` pin 16 | `D` |
| `PIN_E` | `U5` pin 6 | `U5` pin 14 | `E` |
| `PIN_CLK` | `U5` pin 8 | `U5` pin 12 | `CLK` |
| `PIN_LAT` | `U5` pin 17 | `U5` pin 3 | `LAT` |
| `PIN_OE` | `U5` pin 15 | `U5` pin 5 | `OE` |

Both output-enable pins of `U4` are tied to `GND`, so all `U4` outputs are always enabled. `U5` output-enable pins are also tied to `GND`, so all used `U5` outputs are always enabled.

### Mouth Firmware Notes

- The matrix interface is HUB75-like: RGB data for two scan halves (`R1/G1/B1` and `R2/G2/B2`), row address lines `A/B/C/D/E`, pixel clock `CLK`, latch `LAT`, and output enable `OE`.
- `OE` has a 10 kOhm pull-up to `+5V` after the level shifter. Firmware should drive `PIN_OE` deliberately during initialization to avoid unwanted display output.
- All matrix outputs go through 22 Ohm series resistors after the level shifters.
- `D1` is a firmware-controllable status LED on `GPIO2`.
- `D2` and `D3` are power indicator LEDs on `+5V` and `+3.3V` rails.
- USB D+/D- pins on RP2040 are unconnected; programming/debug should use SWD/UART unless a bootloader is already present in flash.

## Suggested Firmware Interface Summary

### Brain ESP32-S3

| Interface | Signals | Pins |
|---|---|---|
| I2C bus | `SDA`, `SCL` | `IO8`, `IO9` |
| Ethernet SPI | `EthCS`, `MOSI`, `SCLK`, `MISO`, `INTn`, `RSTn` | `IO10`, `IO11`, `IO12`, `IO13`, `IO14`, `IO15` |
| Stepper | `STEP`, `DIR`, `TmcEN`, `UART`, `DIAG` | `IO16`, `IO17`, `IO18`, `IO21`, `IO38` |
| I2S speaker/mic | `BCLK`, `LRCLK`, `DIN`, `MicDATA` | `IO39`, `IO40`, `IO41`, `IO47` |
| Power control | `5V_EN`, `5VHP_EN`, `CE`, `BQINT`, `ALRT` | `IO1`, `IO2`, `IO6`, `IO7`, `IO42` |
| USB | `D-`, `D+` | native USB pins |
| Status LED | `D1` | `IO48` |

### Eye RP2040

| Interface | Signals | Pins |
|---|---|---|
| I2C to brain | `SDA`, `SCL` | `GPIO4`, `GPIO5` |
| Display SPI/control | `CS`, `SCK`, `MOSI`, `DC`, `RST`, `BL_PIN` | `GPIO17`, `GPIO18`, `GPIO19`, `GPIO20`, `GPIO21`, `GPIO22` |
| Debug UART | `TX`, `RX` | `GPIO0`, `GPIO1` |
| SWD | `SWCLK`, `SWDIO`, `RUN` | RP2040 SWD pins and `RUN` |
| Status LED | `D1` | `GPIO2` |

### Mouth RP2040

| Interface | Signals | Pins |
|---|---|---|
| I2C to brain | `SDA`, `SCL` | `GPIO4`, `GPIO5` |
| Matrix data | `R1`, `G1`, `B1`, `R2`, `G2`, `B2` | `GPIO6`, `GPIO7`, `GPIO8`, `GPIO9`, `GPIO10`, `GPIO11` |
| Matrix address | `A`, `B`, `C`, `D`, `E` | `GPIO13`, `GPIO14`, `GPIO15`, `GPIO16`, `GPIO12` |
| Matrix timing | `CLK`, `LAT`, `OE` | `GPIO17`, `GPIO18`, `GPIO19` |
| Debug UART | `TX`, `RX` | `GPIO0`, `GPIO1` |
| SWD | `SWCLK`, `SWDIO`, `RUN` | RP2040 SWD pins and `RUN` |
| Status LED | `D1` | `GPIO2` |

## Open Assumptions

- Brain connectors `J2`, `J4`, `J5`, and `J6` will be wired to the two eye displays and the mouth matrix in the final assembly. They are all electrically equivalent I2C/power ports and can be used interchangeably; the firmware distinguishes left eye, right eye, and mouth by I2C slave address, not by which brain connector is used.
- I2C addresses/protocols for the RP2040 coprocessors are a firmware design decision; the schematics only define the electrical bus.
- The eye display part is named `LH128` in the schematic. Its exact controller/protocol must be confirmed from the display module datasheet if initialization commands are not already known.
