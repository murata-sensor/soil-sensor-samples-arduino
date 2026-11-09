# Wiring an SLT5006 to M5Core2

English | [日本語](m5stack-wiring.ja.md)

This guide shows how to connect one SLT5006 with a **3.3 V logic-level UART
interface** to the red **PORT.A** built into an M5Stack Core2 or the blue
**PORT.C** on an M5GO Bottom2 or equivalent. The SLT5005 uses an RS-232C
interface. Even though it looks similar, do not connect it with this wiring.

## Safety first

> Disconnect USB and turn off the Core2, sensor, and external supply before
> connecting, disconnecting, or moving any wire. Do not change the wiring while
> power is on.

1. Check that the sensor label reads `SLT5006`.
2. Check the supply, UART input and output levels, Enable pin, power-up
   sequence, and cable specifications in the latest datasheet.
3. Keep the sensor disconnected while uploading a sketch to the Core2.
4. Wire with the power off. Before powering up, recheck for shorts, polarity,
   and crossed TX/RX.
5. Insulate each unused wire separately instead of bundling them.

The SLT5006 operates from a 3.0-6.0 V supply and draws up to 50 mA during
read/write. One UART supports one sensor, and the maximum cable length is 3 m.
The UART pin logic levels and absolute maximum ratings are specified separately
from the supply. For details, see the latest
[electrical specifications](https://www.murata.com/en-global/products/sensor/soil/design-development/lineup)
and [datasheet download](https://www.murata.com/en-global/products/sensor/soil/design-development/dl).

## Core2 port options

With a standalone Core2, use the red PORT.A as the UART. Both examples use
PORT.A by default. To use the blue PORT.C on an M5GO Bottom2 or equivalent,
select it in the sketch.

| Core2 PORT.A conductor | GPIO | UART role | Example role |
|---|---:|---|---|
| Black | GND | Ground | Common ground |
| Red | 5 V | Power | SLT5006 supply |
| Yellow | GPIO32 | RX | Core2 RX |
| White | GPIO33 | TX | Core2 TX |

| Core2 PORT.C conductor | GPIO | UART role | Example role |
|---|---:|---|---|
| Black | GND | Ground | Common ground |
| Red | 5 V | Power | SLT5006 supply |
| Yellow | GPIO14 | TXD2 | Core2 TX |
| White | GPIO13 | RXD2 | Core2 RX |

This mapping follows the official M5Stack
[Core2](https://docs.m5stack.com/en/core/core2) and
[M5GO Bottom2](https://docs.m5stack.com/en/base/m5go_bottom2) pin maps. Check
the actual Bottom or expansion module and its documentation to confirm that it
has a blue PORT.C with the same pin assignment.

The SLT5006 cable and the Grove cable use some of the same colors for different
signals. **SLT5006 WHITE is Enable**, but the **PORT.C white conductor is
GPIO13/RX**. Do not connect wires just because their colors match.

## Connections

| SLT5006 lead | Function | Connect to |
|---|---|---|
| RED | Sensor supply | Red/5 V conductor on the selected Grove cable |
| BLACK | GND | Black/GND conductor on the selected Grove cable |
| WHITE | Enable, active high | SLT5006 RED in these examples |
| BLUE | Sensor TX | PORT.A yellow/GPIO32 or PORT.C white/GPIO13 |
| YELLOW | Sensor RX | PORT.A white/GPIO33 or PORT.C yellow/GPIO14 |
| GRAY / shield | Shield | Common GND (recommended for stable communication) |
| GREEN | Internal use | Leave disconnected and insulate separately |
| ORANGE | Internal use | Leave disconnected and insulate separately |

```text
SLT5006 BLUE   (Sensor TX) --> PORT.A yellow/GPIO32 or PORT.C white/GPIO13
SLT5006 YELLOW (Sensor RX) <-- PORT.A white/GPIO33 or PORT.C yellow/GPIO14
SLT5006 BLACK  (GND)       --------------------------------- Core2 GND
SLT5006 RED    (Supply)    <--------------------------------- Core2 5 V
SLT5006 WHITE  (Enable)    --------------------------------- SLT5006 RED
```

The UART settings are 9600 bps, 8 data bits, no parity, 1 stop bit, and no
flow control (8N1). The SLT5006 has no device address, so connect it
one-to-one. Do not connect two sensor TX outputs to the same Core2 RX input.

## Power and logic levels

The SLT5006 runs from a 3.0-6.0 V supply through its internal DC-DC converter,
and its UART uses 3.3 V logic. The Core2 ESP32 GPIOs also use 3.3 V logic, so
this combination needs no UART level conversion. Check the following for each
circuit:

- VIH, VIL, VOH, VOL, and absolute maximum ratings of the SLT5006 UART RX/TX
- I/O ratings of the GPIOs in use (GPIO32/GPIO33 or GPIO13/GPIO14)
- Powered-off protection that prevents back-powering through the signal lines
  when only the Core2 or only the sensor is off
- Sensor supply capacity, including startup and transients, and the power-up
  sequence of both devices

You can power the sensor from the red (5 V) conductor of the Grove cable. In
that case, check the Core2 or Bottom output capacity, startup transient
current, and the power-up sequence, including when USB is connected. Do not
power the sensor from a GPIO.

## Safe upload and power-up sequence

1. Disconnect the sensor and its supply from the Core2.
2. Upload `M5Core2QuickStart` or `M5Core2CloudLogger`.
3. Unplug USB and switch the Core2 off.
4. Wire the sensor, including its supply, as shown in the tables above.
5. Recheck that BLACK, supply GND, and Core2 GND share a common ground, that
   polarity is correct, that TX/RX are crossed, and that GREEN and ORANGE are
   insulated separately.
6. Power up in the order defined by the product documentation and your circuit
   design. Treat reconnecting USB as powering the Core2.

Maintenance, a loose connector, a flat battery, or plugging and unplugging USB
can leave only one side powered. In production designs, check for
back-powering in these failure and maintenance cases, not only at normal
startup.

## Selecting PORT.A or PORT.C

Set `SENSOR_PORT` near the top of each sketch to match the connector in use:

```cpp
constexpr SensorPort SENSOR_PORT = SensorPort::PORT_A;  // Core2 G32/G33
// constexpr SensorPort SENSOR_PORT = SensorPort::PORT_C;  // Bottom2 G13/G14
```

PORT.A selects RX=GPIO32 and TX=GPIO33. PORT.C selects RX=GPIO13 and TX=GPIO14.

- While PORT.A is used as a UART, you cannot use an I2C device on PORT.A at the
  same time.
- If the selected port does not match the physical wiring, sensor responses
  time out.

The SLT5006 requirements for supply, signal levels, crossed TX/RX, and Enable
stay the same whichever GPIOs you use.

## If the sensor does not respond

Turn the power off, then check the following in order.

1. Is the sensor an SLT5006, and is only one sensor connected?
2. Is GND common, including BLACK? Is WHITE (Enable) high?
3. Is BLUE (sensor TX) connected to the selected RX (PORT.A GPIO32 or PORT.C
   GPIO13), and YELLOW (sensor RX) to the selected TX (PORT.A GPIO33 or PORT.C
   GPIO14)?
4. Do the GPIOs in the sketch match the actual wiring, with no mix of 32/33 and
   13/14?
5. Is the UART set to 9600 bps, 8N1?
6. Do the supply voltage and UART logic levels meet the specifications? If you
   added a level shifter, is its direction correct?
7. Is there any problem with contacts, cable length, shielding, noise, or
   ground-potential differences?

After wiring, see the [logger setup](logger-setup.md) to check operation.
Protocol details are in [the SLT5006 protocol reference](m5stack-protocol.md).
