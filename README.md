# soil-sensor-samples-arduino

English (default) | [日本語](README.ja.md)

Arduino and M5Stack samples with a reusable UART driver for one Murata **SLT5006** soil sensor.

| Platform | First example | Optional advanced example |
|---|---|---|
| Arduino Uno / classic Nano | `UnoBasicMeasurement` | `UnoCooperativePolling` |
| M5Stack Core2 | `M5Core2QuickStart` | `M5Core2CloudLogger` |

Start with the first example for your board. The advanced examples serve
different purposes: `UnoCooperativePolling` demonstrates an alternative
measurement-control API, while `M5Core2CloudLogger` is a reference application
for recording and uploading measurements. Neither is required for the first
sensor check.

### Example naming

Examples currently use `Board + Purpose`, so the target is visible in Arduino
IDE: `UnoBasicMeasurement` and `M5Core2QuickStart`. While SLT5006 is the only
supported product, the product number is omitted to keep names short. When a
second product is added, all examples will use `Product + Board + Purpose`, for
example `SLT5006UnoBasicMeasurement` and `SLT5009M5Core2QuickStart`.

> This is a software quick start. A hardware designer who can review the current SLT5006 product documents must prepare or approve the power, UART level-conversion, and power-sequencing circuit. Once that circuit is ready, the beginner example runs without code changes.

Detailed English guides cover [wiring](docs/wiring.en.md), the [UART protocol](docs/protocol.en.md), and [application integration](docs/integration.en.md). Japanese versions are also included.

This code is for the **SLT5006 TTL UART interface and one connected sensor only**. Do not connect the RS-232C SLT5005 directly to an Arduino UART. See Murata's official [lineup and electrical specifications](https://www.murata.com/en-global/products/sensor/soil/design-development/lineup) and [datasheet download](https://www.murata.com/en-global/products/sensor/soil/design-development/dl).

## Safety before wiring

> Turn off the sensor, Arduino, and external supply before connecting or disconnecting any wire. Do not hot-plug the sensor.

- The published sensor supply range is 3.0–6.0 V and operating current is up to 50 mA during a read/write cycle. Check the current datasheet for startup/transient current, pins, UART logic levels, and power sequencing.
- An Uno TX uses 5 V logic. Do not assume the SLT5006 RX is 5 V tolerant. Use direction-appropriate level conversion for push-pull UART signals when the levels differ; do not assume an I2C-only MOSFET module is suitable.
- Use an interface with powered-off protection, or keep both UART directions high-impedance until both rails are stable. Do not leave only one side powered while the signal wires remain connected unless the approved interface prevents current injection in both directions.
- Use a supply with adequate current margin. Do not power the sensor from an Arduino I/O pin. Join the sensor-supply and Arduino grounds.
- Insulate each unused wire separately.

This repository does not certify a particular power supply, level shifter, or power sequence. A hardware designer must verify the complete evaluation circuit against both products' ratings.

## Installation

Install **Murata Soil Sensor** with Arduino IDE Library Manager when available.
The IDE will offer to install `M5Unified` and `ArduinoJson`; accept the offered
dependencies. Uno examples do not include these libraries in the firmware.

For a prerelease ZIP, use **Sketch → Include Library → Add .ZIP Library**. ZIP
installation may not install dependencies automatically. M5Stack users should
then install `M5Unified` 0.2.x and `ArduinoJson` 7.x from Library Manager.
Missing headers are reported at compile time; see the troubleshooting table below.

Do not open an `.ino` file directly from an extracted repository. Arduino IDE
will not add the repository's `src/` directory to the library search path.

## What you need for Uno

- One SLT5006
- Arduino Uno R3 or classic Nano for the first check
- Arduino IDE 2.x and the `Arduino AVR Boards` package
- A data-capable USB cable
- A sensor supply that meets the current product requirements
- Push-pull UART level conversion with powered-off protection when required

No third-party Arduino library is required at runtime; the Uno example uses the bundled `SoftwareSerial` library.

## Uno wiring example

The table shows signal routing, not permission to connect unmatched voltage levels directly.

| SLT5006 wire | Function | Arduino / supply side |
|---|---|---|
| RED | Sensor supply | Rated 3.0–6.0 V supply |
| BLACK | Ground | Arduino GND and supply GND |
| WHITE | Enable, active high | Connect to RED for this sample |
| BLUE | Sensor TX | To Uno D10 (RX), through level conversion if required |
| YELLOW | Sensor RX | From Uno D11 (TX), through level conversion if required |
| GRAY / shield | Shield | Ground connection recommended for stable communication |
| GREEN / ORANGE | Internal use | Leave floating and insulate separately |

UART settings are **9600 bps, 8 data bits, no parity, 1 stop bit (8N1)**. TX and RX cross. The official maximum UART cable length for SLT5006 is 3 m.

Controlling WHITE from a GPIO is outside these samples. Verify the Enable pin thresholds, absolute maximum ratings, GPIO voltage, and unpowered-state injection current before designing that connection.

## Uno safe quick start

1. On GitHub, choose **Code → Download ZIP**; do not extract it.
2. In Arduino IDE Boards Manager, install `Arduino AVR Boards` and restart the IDE.
3. Use **Sketch → Include Library → Add .ZIP Library** and select the downloaded ZIP. See [Arduino's ZIP library instructions](https://support.arduino.cc/hc/en-us/articles/5145457742236-Add-libraries-to-Arduino-IDE).
4. Keep the sensor disconnected. Open **File → Examples → Murata Soil Sensor → UnoBasicMeasurement**.
5. Select Arduino Uno and the correct port. First Verify, then Upload. For a classic Nano, also select the correct Processor/bootloader. See [Arduino's upload instructions](https://support.arduino.cc/hc/en-us/articles/4733418441116-Upload-a-sketch-in-Arduino-IDE).
6. Disconnect USB and turn every supply off after upload.
7. Wire the sensor through the reviewed supply/interface circuit, then check polarity and shorts.
8. Apply power using the sequence approved for the sensor and interface. Do not power only one connected UART side without powered-off protection.
9. Open Serial Monitor at **115200 bps**.

Firmware and serial-number output plus CRC-valid frames confirm communication. Measurement validity is a separate check: install the sensor correctly in a known medium and evaluate values against the current product specifications and Murata's [recommended installation method](https://www.murata.com/en-global/products/sensor/soil/design-development/institution).

## M5Stack Core2 quick start

1. Follow [M5Stack's board management instructions](https://docs.m5stack.com/en/arduino/arduino_board):
  add `https://static-cdn.m5stack.com/resource/arduino/package_m5stack_index.json`
  to **Preferences → Additional Boards Manager URLs**, then install `M5Stack`
  in Boards Manager.
2. Install this library and accept its prompted dependencies. For ZIP installs,
  install `M5Unified` 0.2.x and `ArduinoJson` 7.x in Library Manager.
3. Keep the sensor disconnected. Open **File → Examples → Murata Soil Sensor → M5Core2QuickStart**.
4. Select `M5Core2`, then Verify and Upload.
5. Remove USB and wire the powered-off system according to the
  [M5Stack wiring guide](docs/m5stack-wiring.md). The example supports Port A (GPIO32/33)
  and Port C (GPIO13/14); select `SENSOR_PORT` near the top of the sketch.
6. Apply power using the approved sequence and open Serial Monitor at 115200 bps.

The quick-start display uses fixed label and unit columns with right-aligned
numeric values. Before the first successful measurement, it shows `--` instead.
Automatic measurements start every five seconds, or as soon as the current
measurement finishes if it takes longer. The bottom touch buttons are A, B, C
from left to right. `Button A: manual measurement` requests a measurement
without waiting for the next interval; if one is running, the request waits
for it to finish. The next interval is measured from the manual measurement's
start. B and C have no assigned action in the quick start.
`Supply` is the supply voltage reported by the SLT5006, not the Core2 battery
voltage. A separate shaded `Core2 battery (est.)` strip shows the Core2 battery
level estimate and battery voltage, refreshed every five seconds independently
of sensor measurements. The percentage is a guide, not an accurate remaining
capacity measurement; load and charging can affect it. If a reading is unavailable,
the strip says `unavailable` and the failure is reported in Serial Monitor.
On AXP192-equipped Core2 units, the percentage is estimated from battery
voltage. The regulated sensor supply can stay near 5 V as the battery discharges,
so `Supply` is not a Core2 battery-level indicator.

After the quick start works, `M5Core2CloudLogger` adds local CSV logging and
optional Google Sheets upload. Follow the [GAS setup guide](docs/gas-setup.md)
and [logger setup guide](docs/logger-setup.md). Settings are provisioned over USB
into internal NVS, with microSD configuration used when NVS is unconfigured.
microSD also stores logs and pending rows. Neither configuration store is encrypted
by this sample; protect the device and rotate exposed credentials.

## PlatformIO

This repository includes PlatformIO environments for Arduino Uno and M5Stack Core2:

```sh
pio run -e uno_basic
pio run -e uno_cooperative_polling
pio run -e core2_quick_start
pio run -e core2_cloud_logger
```

To upload `UnoBasicMeasurement`, keep the sensor disconnected from the Arduino while uploading:

```sh
pio run -e uno_basic -t upload
```

After uploading, disconnect USB and turn off the Arduino, sensor, and external supply before wiring them according to the [wiring guide](docs/wiring.en.md). Treat reconnecting USB as powering the Arduino side: reconnect it only at the point defined by the approved circuit's power-up sequence, with powered-off protection or both UART directions held high-impedance until both rails are stable. When both sides are powered and the USB data connection is active, open the monitor with:

```sh
pio device monitor -b 115200
```

Use `uno_cooperative_polling` instead of `uno_basic` to upload the cooperative example. The root `platformio.ini` and wrappers under `pio/` exist only to build the sketches under `examples/`; inspect and edit the sketches in `examples/`. Do not copy the repository test configuration into an application project.

To use the library from another PlatformIO project, declare the published Git repository in `lib_deps`:

```ini
[env:uno]
platform = atmelavr
board = uno
framework = arduino
monitor_speed = 115200
lib_deps =
  https://github.com/murata-sensor/soil-sensor-samples-arduino.git
```

In a PlatformIO `.cpp` source file, include `Arduino.h` before `SLT5006.h`. The Git URL or a local-folder dependency works without PlatformIO Registry publication. The verified build targets are Arduino Uno on Atmel AVR and M5Stack Core2 on ESP32; validate other boards separately.

## Examples

### First sensor check

- [`UnoBasicMeasurement`](examples/UnoBasicMeasurement/UnoBasicMeasurement.ino): beginner example that reads identity, waits for each measurement to finish, and waits about five seconds before the next measurement.
- [`M5Core2QuickStart`](examples/M5Core2QuickStart/M5Core2QuickStart.ino): displays sensor identity and measurements on M5Stack Core2, including all three VWC conversions (general soil, rock wool, and coco peat) regardless of sensor firmware. Coco-peat pore EC is displayed only on supported firmware. It uses cooperative polling to keep button handling and battery-display updates running during conversion waiting.

### Optional advanced examples

- [`UnoCooperativePolling`](examples/UnoCooperativePolling/UnoCooperativePolling.ino): reports the same sensor identity and measurement fields as `UnoBasicMeasurement`, but starts measurements and polls for completion separately while blinking the built-in LED during conversion waiting. Use it when other loop work must continue; it is not required to learn basic sensor use. UART calls still block as described below.
- [`M5Core2CloudLogger`](examples/M5Core2CloudLogger/M5Core2CloudLogger.ino): reference application for SD/CSV logging, Wi-Fi, and resilient Google Sheets uploads. It requires additional logger setup; it is not a minimal measurement example.

The default 30-second measurement timeout is a limit for waiting for conversion,
not the normal measurement duration.

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
| `batteryVoltageV` | Converted sensor supply reading | V |

Choose fields for the actual medium and desired physical quantity; do not treat the three VWC conversions as independent simultaneous readings. Firmware before 1.7.5 still returns word 9 as zero, but does not support coco-peat pore EC as a measurement. Check `SLT5006::supportsEcPoreCoco(info)` and the application conditions in the product documentation before displaying or storing the value.

Published representative ranges/resolutions are: EC 0–5 dS/m at 0.001 dS/m, temperature -20–60 degC at 0.0625 degC, and VWC 0–60% for general soil or 0–100% for artificial media at 0.1%. Consult the current datasheet for accuracy, installation, saturation, out-of-range behavior, and application conditions. The driver does not clamp received values.

## Application integration

The driver accepts any Arduino `Stream`. Prefer a hardware UART such as `Serial1` in production systems.

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

`readMeasurement()` is the simple blocking API. To avoid waiting for the entire conversion in one call, use this sequence:

1. `startMeasurement()`
2. Repeatedly call `measurementComplete(complete)`
3. When `complete` is true, call `readMeasurementData()`

This is cooperative conversion waiting, not fully asynchronous UART I/O. The start call, each due status poll, and the final 18-byte value read each use one transaction and can block for one response timeout (about 300 ms with defaults). Budget both waits if a status poll and value read run back-to-back. Use a dedicated task or a fully asynchronous UART state machine when stricter response latency is required.

## Troubleshooting

| Error text | First checks |
|---|---|
| `SLT5006.h: No such file or directory` | Install the GitHub download with **Add .ZIP Library**, then open the example from **File → Examples → Murata Soil Sensor** |
| `M5Unified.h: No such file or directory` / `ArduinoJson.h: No such file or directory` | In Library Manager, install M5Unified 0.2.x (including M5GFX) and ArduinoJson 7.x, then retry Verify |
| `No response from SLT5006 (timeout).` / `sensor response timed out` | No response arrived before the deadline. This is expected when the sensor is disconnected. If connected, turn all power off, then check sensor power, WHITE (Enable), common GND, BLUE to D10, YELLOW from D11, and UART logic levels |
| `sensor response CRC did not match` | Logic levels, 9600 bps, connection quality, noise, shield ground |
| `sensor response format was invalid` | Correct SLT5006 product, one sensor only, stale/noisy serial data |
| `sensor returned a device error` | Record the following hexadecimal code and description |
| `measurement did not finish in time` | Supply stability and sensor state; retry in a later cycle |

Always turn power off before changing wiring.

## Protocol and validation

- Read function `0x01`, write function `0x02`
- CRC-16/MODBUS calculation, but transmitted **high byte then low byte**
- Little-endian multi-byte measurements
- Function, register, size, CRC, and write-echo validation
- Fixed maximum 31-byte response buffer; no heap allocation or exceptions
- Rollover-safe `millis()` timeout arithmetic

The protocol and conversions are implemented from the SLT5006 product documentation. Host tests cover known frames, fragmented reads, CRC/device/protocol errors, conversion, settle/poll timing, and timer rollover. Public CI builds both Uno examples and both M5Stack Core2 examples.

This validation did not include a Hardware-in-the-Loop run with an Arduino Uno or M5Stack Core2 and SLT5006. Before deployment, validate continuous measurement, disconnect/recovery, and dry/wet response with the actual sensor, board, power supply, and level-conversion circuit.

## Development and release checks

Public CI runs host tests on Linux, macOS, and Windows; builds all examples for
the pinned Arduino Uno and M5Stack Core2 platforms; and checks formatting, static analysis,
metadata, example paths, and documentation links. See [the host-test guide](tests/README.md)
for local commands.
It also installs the exported library ZIP and builds all four sketches with
Arduino CLI 1.5.1, Arduino AVR Boards 1.8.6, and M5Stack Boards 3.3.10.
Logger tests also check JSON/CSV formats, SD write failures, queue validation,
and NVS/USB configuration. Python and Apps Script tests cover provisioning,
TLS trust anchors, and receiver validation.

Release changes must keep `library.properties` and `library.json` versions in
sync and publish notes in [GitHub Releases](https://github.com/murata-sensor/soil-sensor-samples-arduino/releases). Report general problems according
to [CONTRIBUTING.md](CONTRIBUTING.md) and vulnerabilities according to
[SECURITY.md](SECURITY.md).

## License

BSD-3-Clause; see [LICENSE](LICENSE). The samples are provided as-is for evaluation and reference. Confirm all behavior against the current product documents before production use.
