# SLT5006 wiring guide

English | [日本語](wiring.md)

## Connect safely

1. Check that the sensor is an SLT5006. The SLT5005 uses RS-232C electrical
   levels, so it cannot be connected with this UART wiring.
2. Turn off the sensor, the Arduino, and the external supply.
3. Check the supply, UART signal, cable, and power-up sequence requirements in
   the latest [electrical specifications](https://www.murata.com/en-global/products/sensor/soil/design-development/lineup)
   and [datasheet](https://www.murata.com/en-global/products/sensor/soil/design-development/dl).
4. Connect power and common ground first, then the signal lines.
5. Insulate each unused wire separately.
6. Check for shorts and correct polarity. Then power up in the order defined by
   the product documentation and your circuit design.

Do not connect or disconnect any wire while power is on.

## Cable signals

| Color | SLT5006 function | Connect to |
|---|---|---|
| RED | Supply | 3.0–6.0 V supply |
| BLACK | Ground | Sensor-supply GND and Arduino GND |
| WHITE | Enable (high: operating, low: standby) | RED in these samples |
| BLUE | Sensor TX (3.3 V logic) | Arduino RX through a 3.3 V to 5 V level shifter |
| YELLOW | Sensor RX (3.3 V logic) | Arduino TX through a 5 V to 3.3 V level shifter |
| GREEN | Internal use | Leave unconnected and insulate separately |
| ORANGE | Internal use | Leave unconnected and insulate separately |
| GRAY / shield | Shield | GND (recommended for stable communication) |

On the Arduino Uno R3, `UnoBasicMeasurement` uses D10 as RX and D11 as TX. The
Uno R3 I/O pins use 5 V logic.

```text
SLT5006 BLUE  (TX, 3.3 V) -- level conversion ------> Uno D10 (RX, 5 V)
SLT5006 YELLOW(RX, 3.3 V) <-- level conversion ------ Uno D11 (TX, 5 V)
SLT5006 BLACK (GND) --------------------------------- Uno GND
sensor supply GND   --------------------------------- Uno GND
```

## Supply and signal voltage

The SLT5006 UART uses 3.3 V logic, and the Arduino Uno R3 I/O uses 5 V logic.
Because the levels differ, add level conversion in both UART directions.
Convert Uno TX (D11) from 5 V to 3.3 V for sensor RX (YELLOW), and convert
sensor TX (BLUE) from 3.3 V to 5 V for Uno RX (D10).

Use a level shifter that supports 9600 bps push-pull UART signals. Do not use
an I2C-only bidirectional MOSFET module. For other Arduino boards, choose the
conversion directions from that board's UART voltage and I/O specifications.

The supply voltage and the UART signal voltage are specified separately. Keep
the sensor supply within 3.0–6.0 V, and make sure it can deliver the 50 mA
maximum read/write current plus startup and transient current. Do not power
the sensor from an Arduino I/O pin. If you power it from a supply pin on the
Arduino board, check the current capacity of the board, USB source, regulator,
and wiring.

If you control WHITE (Enable) from a GPIO, stay within the Enable input
thresholds and absolute maximum ratings. Do not connect a 5 V Uno GPIO
directly. Use a driver or level shifter that meets the Enable input
conditions. These samples do not use standby control, so WHITE is tied to RED.

## Upload, wiring, and power-up sequence

If only the Arduino is powered, current can flow from its TX line into the
unpowered sensor. Do not power only the Arduino over USB while the sensor is
connected.

1. Upload the sketch with the SLT5006 disconnected.
2. Unplug USB and turn off the Arduino, the sensor, and the external supply.
3. Wire the power supply, common ground, and level shifters.
4. Use a circuit that prevents back-powering through the signal lines when
   either side is off. Alternatively, hold both UART directions at high
   impedance until both supplies are stable.
5. Power up in the order defined by the product documentation and your circuit
   design.

We have not qualified any particular power supply or level shifter. Build the
supply and level-conversion circuit to meet the ratings of both the SLT5006
and the Arduino.

## Deployment notes

- The Uno shares D0/D1 with the USB serial monitor, so the quick start uses
  `SoftwareSerial` on D10/D11.
- `SoftwareSerial` can receive on only one port at a time. Libraries that rely
  heavily on interrupts can interfere with it.
- On boards with a hardware UART, prefer `Serial1` or another hardware UART.
  Check the board documentation for its pins.
- Connect only one SLT5006 to each UART. Do not connect multiple sensor TX
  outputs to the same RX line.
- The maximum SLT5006 UART cable length is 3 m. Within that length, noise,
  routing, shielding, and ground-potential differences still affect
  communication.
