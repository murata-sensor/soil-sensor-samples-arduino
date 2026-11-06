# SLT5006 wiring guide

English | [日本語](wiring.md)

## Connect safely

1. Confirm that the product is an SLT5006. The SLT5005 uses RS-232C electrical
   levels and must not use this Arduino UART connection.
2. Turn off the sensor, Arduino, and external supply.
3. Check the current official [lineup and electrical specifications](https://www.murata.com/en-global/products/sensor/soil/design-development/lineup)
   and [datasheet downloads](https://www.murata.com/en-global/products/sensor/soil/design-development/dl)
   for supply, current, UART levels, cable, and power-sequence requirements.
4. Connect power and common ground, then the signal interface.
5. Insulate every unused wire separately.
6. Recheck polarity and shorts, then apply power in the sequence approved for
   the selected interface circuit.

Do not connect or disconnect wires while powered.

## Cable signals

| Color | SLT5006 function | Connection |
|---|---|---|
| RED | Supply | Supply meeting the current 3.0–6.0 V requirement |
| BLACK | Ground | Sensor-supply GND and Arduino GND |
| WHITE | Enable, active high | Connect to RED for these samples |
| BLUE | Sensor TX | Arduino RX through level conversion when required |
| YELLOW | Sensor RX | Arduino TX through level conversion when required |
| GREEN | Internal use | Leave floating and insulate |
| ORANGE | Internal use | Leave floating and insulate |
| GRAY / shield | Shield | Grounding is recommended for stable communication |

`UnoBasicMeasurement` uses D10 as Arduino RX and D11 as Arduino TX:

```text
SLT5006 BLUE  (TX) -- level conversion if required --> Uno D10 (RX)
SLT5006 YELLOW(RX) <-- level conversion if required -- Uno D11 (TX)
SLT5006 BLACK (GND) --------------------------------- Uno GND
sensor supply GND   --------------------------------- Uno GND
```

## Supply and signal voltage

Supply voltage and UART logic voltage are separate specifications. A sensor
supply inside 3.0–6.0 V does not imply that the sensor UART input accepts the
Uno's 5 V output. Check each of these independently:

- supply voltage, startup/transient current, and operating current margin;
- SLT5006 UART input/output levels and absolute maximum ratings;
- Arduino UART signal voltage;
- direction and voltage compatibility of both push-pull UART paths;
- powered-off protection, or a circuit that keeps both UART paths high
  impedance until both rails are stable.

Do not power the sensor from an Arduino I/O pin. If an Arduino board supply pin
is considered, verify the board, USB source, regulator, wiring, startup, and
transient capacity. The public operating-current value is up to 50 mA during a
read/write cycle, but that value alone is not a complete power-supply design
requirement.

Controlling WHITE from a GPIO requires a separate check of Enable thresholds,
absolute maximum ratings, GPIO voltage, and unpowered-state injection current.
These samples instead use the product-approved active connection of WHITE to
RED and do not implement standby control.

## Upload, wiring, and power sequence

`SoftwareSerial` can drive its TX pin high after Arduino startup. Do not leave
an unpowered sensor connected to an independently USB-powered Arduino unless
the approved interface prevents current injection.

1. Upload the sketch while the sensor is disconnected.
2. Remove USB and turn every supply off.
3. Complete the power, common-ground, and level-conversion wiring.
4. Use powered-off protection or keep both UART directions high impedance until
   both supplies are stable.
5. Apply power in the sequence defined by current product documentation and the
   reviewed circuit.

This repository does not certify a specific supply, level shifter, or sequence.
The circuit designer is responsible for meeting both products' ratings.

## Deployment notes

- D0/D1 on Uno are shared with USB serial, so the quick start uses D10/D11.
- Only one `SoftwareSerial` receiver can listen at once; interrupt-heavy
  libraries can interfere with it. Prefer `Serial1` or another hardware UART
  when the board provides one.
- The SLT5006 connection is one-to-one. Do not connect multiple TX outputs to a
  single RX line.
- The official maximum SLT5006 UART cable length is 3 m.
  Still evaluate noise, routing, shielding, and ground potential difference.
