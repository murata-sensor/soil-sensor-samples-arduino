# soil-sensor-samples-arduino

English (default) | [日本語](README.ja.md)

Arduino and M5Stack samples with a reusable UART driver for one Murata **SLT5006** soil sensor.

| Platform | First example | Optional advanced example |
|---|---|---|
| Arduino Uno / classic Nano | `UnoBasicMeasurement` | `UnoCooperativePolling` |
| M5Stack Core2 | `M5Core2QuickStart` | `M5Core2CloudLogger` |

Start with the first example for your board. The advanced examples have
different purposes: `UnoCooperativePolling` shows an alternative
measurement-control API, and `M5Core2CloudLogger` is a reference application
that records and uploads measurements. You do not need either of them for the
first sensor check.

### Example naming

Example names currently follow `Board + Purpose`, so the target board is
visible in Arduino IDE: `UnoBasicMeasurement` and `M5Core2QuickStart`. While
SLT5006 is the only supported product, the names omit the product number to
stay short. When a second product is added, all examples will switch to
`Product + Board + Purpose`, for example `SLT5006UnoBasicMeasurement` and
`SLT5009M5Core2QuickStart`.

> This is a software quick start. The SLT5006 UART uses 3.3 V logic and Arduino Uno R3 uses 5 V logic, so add level conversion in both UART directions. Follow the product specifications for the power supply and power-up sequence. For other Arduino boards, design the interface to match that board's UART voltage and I/O specifications.

Detailed English guides cover [wiring](docs/wiring.en.md), the [UART protocol](docs/protocol.en.md), and [application integration](docs/integration.en.md). Japanese versions are also available.

This code is for the **SLT5006 TTL UART interface and one connected sensor only**. Do not connect the RS-232C SLT5005 directly to an Arduino UART. See Murata's official [lineup and electrical specifications](https://www.murata.com/en-global/products/sensor/soil/design-development/lineup) and [datasheet download](https://www.murata.com/en-global/products/sensor/soil/design-development/dl).

## Safety before wiring

> Turn off the sensor, Arduino, and external supply before connecting or disconnecting any wire. Do not hot-plug the sensor.

- The sensor supply range is 3.0–6.0 V, and the sensor UART uses 3.3 V logic. Choose a supply that can deliver the 50 mA maximum operating current plus the startup and transient current.
- Arduino Uno R3 I/O uses 5 V logic. Convert both UART directions between the Uno's 5 V and the sensor's 3.3 V.
- Use an interface with powered-off protection, or hold both UART directions at high impedance until both supplies are stable. While the signal wires are connected, do not keep only one side powered unless the interface blocks current injection in both directions.
- Do not power the sensor from an Arduino I/O pin. Connect the sensor-supply ground to the Arduino ground.
- Insulate each unused wire separately.

We have not qualified any particular power supply, level shifter, or power-up sequence. Make sure the complete evaluation circuit meets the ratings of both the SLT5006 and the Arduino.

## Installation

Once **Murata Soil Sensor** is available in the Arduino IDE Library Manager,
install it from there. When the IDE offers to install the `M5Unified` and
`ArduinoJson` dependencies, install them too. The Uno examples do not include
these libraries in their firmware.

For a prerelease ZIP, use **Sketch → Include Library → Add .ZIP Library**. A ZIP
installation might not install the dependencies, so for M5Stack, also install
`M5Unified` 0.2.x and `ArduinoJson` 7.x from Library Manager. Missing headers
are reported as compile errors. See the troubleshooting table below.

Do not open an `.ino` file directly from an extracted copy of the repository.
Arduino IDE does not add the repository's `src/` directory to the library
search path.

## What you need for Uno

- One SLT5006
- Arduino Uno R3 or classic Nano for the first check
- Arduino IDE 2.x and the `Arduino AVR Boards` package
- A data-capable USB cable
- A sensor supply that meets the SLT5006 ratings
- A push-pull UART level shifter that converts between 5 V and 3.3 V in both directions and has powered-off protection

The Uno example needs no third-party Arduino library. It uses the bundled `SoftwareSerial` library.

## Uno wiring example

Uno R3 I/O uses 5 V logic, and the SLT5006 UART uses 3.3 V logic, so add level conversion in both UART directions. Convert the Uno D11 (TX) signal from 5 V to 3.3 V for the yellow sensor-RX wire, and the blue sensor-TX signal from 3.3 V to 5 V for Uno D10 (RX). Use a level shifter designed for push-pull UART signals, not an I2C-only bidirectional MOSFET module. For other Arduino boards, match the level conversion to that board's UART voltage and I/O specifications.

| SLT5006 wire | Function | Arduino / supply side |
|---|---|---|
| RED | Sensor supply | 3.0–6.0 V supply |
| BLACK | Ground | Arduino GND and supply GND |
| WHITE | Enable, active high | Connect to RED for this sample |
| BLUE | Sensor TX (3.3 V) | To Uno D10 (RX) through the level shifter |
| YELLOW | Sensor RX (3.3 V) | From Uno D11 (TX) through the level shifter |
| GRAY / shield | Shield | Ground connection recommended for stable communication |
| GREEN / ORANGE | Internal use | Leave floating and insulate separately |

UART settings are **9600 bps, 8 data bits, no parity, 1 stop bit (8N1)**. Cross TX and RX. The official maximum UART cable length for SLT5006 is 3 m.

These samples do not control WHITE from a GPIO. Before you design such a connection, check the Enable input thresholds, the absolute maximum ratings, the GPIO voltage, and the injection current when one side is unpowered.

## Uno quick start

1. On GitHub, choose **Code → Download ZIP**. Do not extract the ZIP.
2. In Arduino IDE Boards Manager, install `Arduino AVR Boards`, then restart the IDE.
3. Choose **Sketch → Include Library → Add .ZIP Library** and select the downloaded ZIP. See [Arduino's ZIP library instructions](https://support.arduino.cc/hc/en-us/articles/5145457742236-Add-libraries-to-Arduino-IDE).
4. With the sensor still disconnected, open **File → Examples → Murata Soil Sensor → UnoBasicMeasurement**.
5. Select Arduino Uno and the correct port. Click Verify, then Upload. For a classic Nano, also select the correct Processor/bootloader. See [Arduino's upload instructions](https://support.arduino.cc/hc/en-us/articles/4733418441116-Upload-a-sketch-in-Arduino-IDE).
6. After the upload, disconnect USB and turn off every supply.
7. Wire the sensor through a supply and interface circuit that meets the ratings of both products, then check polarity and shorts.
8. Power up in the sequence specified for the sensor and the interface. Without powered-off protection, do not power only one side while the UART wires are connected.
9. Open Serial Monitor at **115200 bps**.

If the firmware version and serial number appear and the frames pass the CRC check, communication is working. Measurement validity is a separate check. Install the sensor in a known medium following Murata's [recommended installation method](https://www.murata.com/en-global/products/sensor/soil/design-development/institution), then evaluate the values against the current product specifications.

## M5Stack Core2 quick start

1. Follow [M5Stack's board management instructions](https://docs.m5stack.com/en/arduino/arduino_board):
  add `https://static-cdn.m5stack.com/resource/arduino/package_m5stack_index.json`
  to **Preferences → Additional Boards Manager URLs**, then install `M5Stack`
  in Boards Manager.
2. Install this library and accept the dependencies that the IDE offers. For a ZIP
  installation, install `M5Unified` 0.2.x and `ArduinoJson` 7.x from Library Manager.
3. With the sensor still disconnected, open **File → Examples → Murata Soil Sensor → M5Core2QuickStart**.
4. Select `M5Core2`, then click Verify and Upload.
5. Disconnect USB and, with all power off, wire the system according to the
  [M5Stack wiring guide](docs/m5stack-wiring.md). The example supports Port A (GPIO32/33)
  and Port C (GPIO13/14). Select the port with `SENSOR_PORT` near the top of the sketch.
6. Power up in the specified sequence, then open Serial Monitor at 115200 bps.

The quick-start display shows labels and units in fixed columns, with numeric
values right-aligned. Until the first measurement succeeds, it shows `--` in
place of the values. A measurement starts automatically every five seconds. If
a measurement takes longer than five seconds, the next one starts as soon as it
finishes. The bottom touch buttons are A, B, and C from left to right.
`Button A: manual measurement` requests a measurement without waiting for the
next interval. If a measurement is already running, the request waits until it
finishes. The next interval is then timed from the start of the manual
measurement. B and C have no function in the quick start.

`Supply` is the supply voltage reported by the SLT5006, not the Core2 battery
voltage. A separate shaded `Core2 battery (est.)` strip shows the estimated
Core2 battery level and the battery voltage. It refreshes every five seconds,
independently of sensor measurements. The percentage is a guide only, not an
accurate measurement of remaining capacity. Load and charging can change it.
If the battery reading fails, the strip shows `unavailable` and Serial Monitor
reports the failure. On Core2 units with an AXP192, the percentage is estimated
from the battery voltage. The sensor supply is regulated and can stay near 5 V
while the battery discharges, so `Supply` does not indicate the Core2 battery
level.

Once the quick start works, you can move on to `M5Core2CloudLogger`, which adds
CSV logging to microSD and optional upload to Google Sheets. Follow the
[GAS setup guide](docs/gas-setup.md) and [logger setup guide](docs/logger-setup.md).
Settings are provisioned over USB and stored in internal NVS. If NVS has no
settings, the logger uses the configuration on microSD. microSD also stores the
logs and the rows waiting to be uploaded. This sample does not encrypt either
configuration store. Protect the device, and rotate any credentials that have
been exposed.

## PlatformIO

This repository includes PlatformIO environments for Arduino Uno and M5Stack Core2:

```sh
pio run -e uno_basic
pio run -e uno_cooperative_polling
pio run -e core2_quick_start
pio run -e core2_cloud_logger
```

To upload `UnoBasicMeasurement`, keep the sensor disconnected from the Arduino and run:

```sh
pio run -e uno_basic -t upload
```

After uploading, disconnect USB and turn off the Arduino, the sensor, and the external supply. Then wire them according to the [wiring guide](docs/wiring.en.md). Reconnecting USB powers the Arduino side, so reconnect it only at the step defined in your circuit's power-up sequence. Until both supplies are stable, rely on powered-off protection or keep both UART directions at high impedance. When both sides are powered and the USB data connection is active, open the monitor:

```sh
pio device monitor -b 115200
```

To upload the cooperative example, use `uno_cooperative_polling` instead of `uno_basic`. The root `platformio.ini` and the wrappers under `pio/` exist only to build the sketches in `examples/`, so read and edit the sketches there. Do not copy this repository's test configuration into your application project.

To use the library in your own PlatformIO project, add the published Git repository to `lib_deps`:

```ini
[env:uno]
platform = atmelavr
board = uno
framework = arduino
monitor_speed = 115200
lib_deps =
  https://github.com/murata-sensor/soil-sensor-samples-arduino.git
```

In a PlatformIO `.cpp` source file, include `Arduino.h` before `SLT5006.h`. A Git URL or local-folder dependency works without a PlatformIO Registry release. Builds are verified for Arduino Uno on Atmel AVR and M5Stack Core2 on ESP32. Validate other boards separately.

## Examples

### First sensor check

- [`UnoBasicMeasurement`](examples/UnoBasicMeasurement/UnoBasicMeasurement.ino): beginner example that reads the sensor identity, waits for each measurement to finish, then waits about five seconds before the next measurement.
- [`M5Core2QuickStart`](examples/M5Core2QuickStart/M5Core2QuickStart.ino): shows the sensor identity and measurements on M5Stack Core2, including all three VWC conversions (general soil, rock wool, and coco peat) on any sensor firmware. Coco-peat pore EC appears only with firmware that supports it. The example uses cooperative polling so that button handling and battery-display updates continue while it waits for a conversion.

### Optional advanced examples

- [`UnoCooperativePolling`](examples/UnoCooperativePolling/UnoCooperativePolling.ino): reports the same sensor identity and measurement fields as `UnoBasicMeasurement`, but starts each measurement and polls for completion in separate steps, blinking the built-in LED while it waits for the conversion. Use it as a reference when other loop work must continue. You do not need it to learn basic sensor use. Each UART call still blocks, as described below.
- [`M5Core2CloudLogger`](examples/M5Core2CloudLogger/M5Core2CloudLogger.ino): reference application for SD/CSV logging, Wi-Fi, and Google Sheets uploads with retries. It needs additional logger setup and is not a minimal measurement example.

The default 30-second measurement timeout is the upper limit for waiting for a
conversion, not the typical measurement time.

## Measurement fields

| Member | Meaning | Unit |
|---|---|---|
| `temperatureC` | Temperature | degC |
| `vwcPercent` | General-soil VWC | % |
| `vwcRockPercent` | Rock-wool VWC | % |
| `vwcCocoPercent` | Coco-peat VWC | % |
| `ecBulkDsM` | Bulk EC | dS/m |
| `ecPoreDsM` | Pore-water EC | dS/m |
| `ecPoreCocoDsM` | Coco-peat pore EC | dS/m |
| `batteryVoltageV` | Supply voltage reported by the sensor (converted value) | V |

Choose the fields that match the actual medium and the quantity you need. The three VWC values are conversions for different media. Do not treat them as independent readings taken at the same time. Firmware before 1.7.5 returns zero in word 9 and does not support coco-peat pore EC. Before you display or store that value, check `SLT5006::supportsEcPoreCoco(info)` and the application conditions in the product documentation.

The published representative ranges and resolutions are EC 0–5 dS/m in 0.001 dS/m steps, temperature -20–60 degC in 0.0625 degC steps, and VWC 0–60% (general soil) or 0–100% (artificial media) in 0.1% steps. For accuracy, installation, saturation, out-of-range behavior, and application conditions, see the current datasheet. The driver does not clamp received values.

## Application integration

The driver accepts any Arduino `Stream`. In production systems, prefer a hardware UART such as `Serial1`.

```cpp
#include <Arduino.h>
#include <SLT5006.h>

SLT5006 sensor(Serial1);

void setup() {
  Serial1.begin(SLT5006::BAUD_RATE);
}

void loop() {
  SLT5006::Measurement value;
  if (sensor.readMeasurement(value)) {
    // Pass value.vwcPercent, etc. to the application.
  } else {
    // Record sensor.lastError() and retry in a later measurement cycle.
  }
}
```

`readMeasurement()` is the simple blocking API. To avoid waiting for the whole conversion in a single call, use this sequence instead:

1. `startMeasurement()`
2. Call `measurementComplete(complete)` repeatedly
3. When `complete` is true, call `readMeasurementData()`

Only the wait for the conversion becomes cooperative. The UART I/O itself is not fully asynchronous. The start call, each status poll when it is due, and the final 18-byte value read are each one transaction, and each can block for up to one response timeout (about 300 ms by default). If a status poll and the value read run back-to-back, allow for both waits. If you need stricter response latency, use a dedicated task or a fully asynchronous UART state machine.

## Troubleshooting

| Error text | First checks |
|---|---|
| `SLT5006.h: No such file or directory` | Install the ZIP downloaded from GitHub with **Add .ZIP Library**, then open the example from **File → Examples → Murata Soil Sensor** |
| `M5Unified.h: No such file or directory` / `ArduinoJson.h: No such file or directory` | Install M5Unified 0.2.x (including M5GFX) and ArduinoJson 7.x from Library Manager, then run Verify again |
| `No response from SLT5006 (timeout).` / `sensor response timed out` | The sensor did not respond before the deadline. This is expected when no sensor is connected. If a sensor is connected, turn off all power, then check the sensor supply, WHITE (Enable), common GND, BLUE to D10, YELLOW from D11, and the UART logic levels |
| `sensor response CRC did not match` | Check the logic levels, the 9600 bps setting, connection quality, noise, and shield grounding |
| `sensor response format was invalid` | Check that the device is an SLT5006 and that only one sensor is connected. Look for stale or noisy serial data |
| `sensor returned a device error` | Record the hexadecimal error code and description shown after this message |
| `measurement did not finish in time` | Check supply stability and the sensor state. Retry in a later cycle |

Always turn off the power before changing the wiring.

## Protocol and validation

- Read function `0x01`, write function `0x02`
- CRC-16/MODBUS calculation, but transmitted **high byte then low byte**
- Little-endian multi-byte measurements
- Function, register, size, CRC, and write-echo validation
- Fixed 31-byte maximum response buffer, with no heap allocation or exceptions
- Rollover-safe `millis()` timeout arithmetic

The protocol and conversions are implemented according to the SLT5006 product documentation. Host tests cover known frames, fragmented reads, CRC/device/protocol errors, conversion, settle/poll timing, and timer rollover. Public CI builds both Uno examples and both M5Stack Core2 examples.

This validation did not include Hardware-in-the-Loop testing with an SLT5006 connected to an Arduino Uno or M5Stack Core2. Before deployment, test continuous measurement, disconnection and recovery, and dry/wet response with the actual sensor, board, power supply, and level-conversion circuit.

## Development and release checks

Public CI runs the host tests on Linux, macOS, and Windows, and builds all
examples for the pinned Arduino Uno and M5Stack Core2 platforms. It also runs
formatting, static-analysis, metadata, example-path, and documentation-link
checks. In addition, it installs the exported library ZIP and builds all four
sketches with Arduino CLI 1.5.1, Arduino AVR Boards 1.8.6, and M5Stack Boards
3.3.10. Logger tests also check JSON/CSV formats, SD write failures, queue
validation, and NVS/USB configuration. Python and Apps Script tests cover
provisioning, TLS trust anchors, and receiver validation. See
[the host-test guide](tests/README.md) for the commands to run tests locally.

For each release, keep the versions in `library.properties` and `library.json`
in sync, and publish release notes in [GitHub Releases](https://github.com/murata-sensor/soil-sensor-samples-arduino/releases).
Report general problems as described in [CONTRIBUTING.md](CONTRIBUTING.md), and
report vulnerabilities as described in [SECURITY.md](SECURITY.md).

## License

BSD-3-Clause. See [LICENSE](LICENSE). The samples are provided as-is for evaluation and reference. Before production use, check all behavior against the current product documents.
