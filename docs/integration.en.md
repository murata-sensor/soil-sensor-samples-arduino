# Application integration guide

English | [日本語](integration.md)

## Select a UART

`SLT5006` accepts an Arduino `Stream&`, including `SoftwareSerial`, `Serial1`,
and compatible UART classes.

```cpp
SLT5006 sensor(Serial1);

void setup() {
  Serial1.begin(SLT5006::BAUD_RATE);
}
```

Prefer a hardware UART in deployed systems. `SoftwareSerial` has board-specific
RX pins, permits only one listening receiver, can conflict with interrupt-heavy
features, and can lose bytes in a heavily loaded loop. Do not access one UART
concurrently from multiple tasks or interrupt contexts. Connect one sensor only.

A custom `Stream` must use current Arduino `flush()` semantics: wait for pending
transmit data, or return immediately for synchronous writes. It must not clear
received bytes. Its `available()` and `read()` implementation must also allow a
timeout to progress when a byte disappears between those two calls.

Plan for partial power loss: isolate the signal with powered-off protection or
make the Arduino TX high impedance while the sensor is unpowered. Include
restart, maintenance, connector failure, and supply failure in the design.

## Blocking and cooperative APIs

For a simple periodic measurement:

```cpp
SLT5006::Measurement value;
if (sensor.readMeasurement(value)) {
  saveValue(value.vwcPercent);
} else {
  logError(sensor.lastError());
}
```

`readMeasurement()` starts a conversion, waits at least 300 ms, polls no more
often than every 200 ms, and stops starting new polls after the default 30-second
conversion deadline.

For cooperative conversion waiting, use this state sequence:

```text
IDLE --startMeasurement()--> MEASURING
MEASURING --measurementComplete(false)--> MEASURING
MEASURING --measurementComplete(true)--> READY
READY --readMeasurementData()--> IDLE
```

Before 300 ms, `measurementComplete()` returns `complete=false` without a UART
request. Calls sooner than the configured poll interval also return without a
request. See [`UnoNonBlockingMeasurement`](../examples/UnoNonBlockingMeasurement/UnoNonBlockingMeasurement.ino).

This avoids blocking for the entire conversion; it does not make UART I/O fully
asynchronous. Start and each due status poll can block for one response timeout.
The final 18-byte data operation uses one transaction and can block for about
300 ms with defaults. Budget both waits if a due status poll and data read run
back-to-back. Use a dedicated task or asynchronous UART state machine
for stricter latency.

## Timeouts and recovery

```cpp
sensor.setResponseTimeout(300);
sensor.setPollInterval(200);
sensor.setMeasurementTimeout(30000);
```

Shorter values can reject normal delays; longer values delay fault detection.
Validate changes with the actual board, UART, wiring, sensor, and temperature
range. Timeout arithmetic uses unsigned subtraction and survives `millis()`
rollover.

The measurement timeout is a conversion-wait deadline, not a strict API
wall-clock limit. A UART response wait begun before that deadline and a blocking
poll delay can extend the call beyond it.

On failure, record `lastError()`, `lastDeviceErrorCode()` when applicable, and a
timestamp. Do not retry commands rapidly in one cycle. Retry from
`startMeasurement()` in a later cycle; investigate persistent errors as power,
ground, UART-level, wiring, or noise problems. The driver drains stale input
before a request to resynchronize after delayed errors or partial frames.

## Firmware capability and measurement selection

Read firmware at startup and retain the capability result:

```cpp
SLT5006::SensorInfo info;
bool hasCocoPoreEc = false;
if (sensor.readInfo(info)) {
  hasCocoPoreEc = SLT5006::supportsEcPoreCoco(info);
}
```

Firmware 1.6.3 does not support `ecPoreCocoDsM`; 1.7.5 and 1.7.6 do. Earlier
firmware still returns a readable zero in word 9, but that zero is not a
supported measurement. If firmware cannot be identified or does not support
the field, do not display, store, or upload it.

Treat communication success separately from measurement validity:

1. Confirm firmware, serial, and CRC-valid frames.
2. Select one VWC conversion for the actual medium.
3. Distinguish bulk EC from pore-water EC.
4. Check firmware and product conditions before using coco-peat pore EC.
5. Follow Murata's [recommended installation](https://www.murata.com/en-global/products/sensor/soil/design-development/institution)
   and evaluate repeatability in a known medium.
6. Define saturation and out-of-range handling in the application. The driver
   does not clamp values.

Do not average the medium-specific conversions as if they were independent
simultaneous measurements.

## Memory and porting checks

The driver uses no dynamic `String`, heap allocation, or exceptions. It uses a
fixed 31-byte maximum response buffer and returns converted `float` values.
Arduino Uno implements `double` with the same 32-bit precision as `float`.

When porting the protocol, test CRC value and high-byte-first wire order,
fragmented responses, four-byte error frames, every response-header and echo
check, signed 12-bit temperature, little-endian values, the 300 ms settle time,
the 30-second deadline, and `millis()` rollover. See the [protocol guide](protocol.en.md).
