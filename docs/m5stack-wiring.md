# Wiring an SLT5006 to M5Core2

English | [日本語](m5stack-wiring.ja.md)

This guide connects **one SLT5006 TTL-UART sensor** to the red **PORT.A** built
into an M5Stack Core2 or the blue **PORT.C** on an M5GO Bottom2 or equivalent.
Do not use this wiring for the RS-232C SLT5005.

## Safety first

> Disconnect USB and turn off the Core2, sensor, and external supply before
> connecting, disconnecting, or moving any wire. Do not hot-plug the wiring.

Confirm the sensor's supply, UART logic levels, Enable ratings, and power
sequence in its current datasheet. Murata lists a 3.0-6.0 V supply, 50 mA
maximum read/write current, one sensor per SLT5006 UART, and a 3 m maximum
interface cable. Supply ratings do not define the UART pin voltage. See the
official [electrical specifications](https://www.murata.com/en-global/products/sensor/soil/design-development/lineup)
and [datasheet download](https://www.murata.com/en-global/products/sensor/soil/design-development/dl).

Upload the sketch while the sensor is disconnected. Wire only after removing
USB and switching all power off.

## Core2 port options

Use red PORT.A with a standalone Core2; this is the default in both examples.
Select PORT.C in the sketch when using blue PORT.C on an M5GO Bottom2 or equivalent.

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

Check that your actual bottom/expansion provides this port and mapping. See the
official [Core2](https://docs.m5stack.com/en/core/core2) and
[M5GO Bottom2](https://docs.m5stack.com/en/base/m5go_bottom2) pin maps.

The two cables reuse color names. In particular, **SLT5006 WHITE is Enable**,
while the **PORT.C white conductor is GPIO13/RX**. Do not join wires merely
because their colors match.

## Connections

| SLT5006 lead | Function | Connect to |
|---|---|---|
| RED | Sensor supply | Red/5 V conductor on the selected Grove cable |
| BLACK | GND | Black/GND conductor on the selected Grove cable |
| WHITE | Enable, active high | SLT5006 RED for these examples |
| BLUE | Sensor TX | PORT.A yellow/GPIO32 or PORT.C white/GPIO13 |
| YELLOW | Sensor RX | PORT.A white/GPIO33 or PORT.C yellow/GPIO14 |
| GRAY / shield | Shield | Common GND recommended for stable communication |
| GREEN | Internal use | Leave disconnected and insulate separately |
| ORANGE | Internal use | Leave disconnected and insulate separately |

```text
SLT5006 BLUE   (Sensor TX) --> PORT.A yellow/GPIO32 or PORT.C white/GPIO13
SLT5006 YELLOW (Sensor RX) <-- PORT.A white/GPIO33 or PORT.C yellow/GPIO14
SLT5006 BLACK  (GND)       --------------------------------- Core2 GND
SLT5006 RED    (Supply)    <--------------------------------- Core2 5 V
SLT5006 WHITE  (Enable)    --------------------------------- SLT5006 RED
```

The UART is 9600 bps, 8 data bits, no parity, one stop bit, and no flow control
(8N1). The SLT5006 has no device address. Never connect two sensor TX outputs
to the same Core2 RX input.

## Power and logic levels

The SLT5006 operates from a 3.0-6.0 V supply through its internal DC-DC and its
UART signals use 3.3 V logic. Core2 ESP32 GPIO also uses 3.3 V logic, so this
combination does not require UART level conversion. When powering the sensor
from Grove red/5 V, confirm the Core2 or Bottom output capacity, startup
transients, and power sequence including USB events. The interface must prevent
back-powering when either side is off. Never power the sensor from a GPIO.

## Safe upload and power-up sequence

1. Disconnect the sensor and its supply from the Core2.
2. Upload `M5Core2QuickStart` or `M5Core2CloudLogger`.
3. Remove USB and switch the Core2 off.
4. Wire the approved supply and any level conversion as shown above.
5. Recheck common ground, crossed TX/RX, polarity, and separately insulated
   GREEN and ORANGE leads.
6. Power the system in the order approved for your circuit. Treat reconnecting
   USB as a Core2 power event.

Also evaluate unplugging, maintenance, a flat battery, and either side losing
power. These are back-powering cases, not just startup details.

## Selecting PORT.A or PORT.C

Set `SENSOR_PORT` near the top of each sketch to match the connector in use:

```cpp
constexpr SensorPort SENSOR_PORT = SensorPort::PORT_A;  // Core2 G32/G33
// constexpr SensorPort SENSOR_PORT = SensorPort::PORT_C;  // Bottom2 G13/G14
```

PORT.A selects RX=GPIO32 and TX=GPIO33. PORT.C selects RX=GPIO13 and TX=GPIO14.
Do not share PORT.A with an I2C device while using it as UART. A mismatch between
the selected port and the physical wiring produces response timeouts.

For operation and troubleshooting, continue with the
[logger setup](logger-setup.md). Protocol details are in
[the SLT5006 protocol reference](m5stack-protocol.md).
