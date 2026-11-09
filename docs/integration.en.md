# Application integration guide

English | [日本語](integration.md)

## Select a UART

`SLT5006` takes an Arduino `Stream&`, so you can pass `SoftwareSerial`,
`Serial1`, or another compatible UART class.

```cpp
SLT5006 sensor(Serial1);

void setup() {
  Serial1.begin(SLT5006::BAUD_RATE);
}
```

Prefer a hardware UART in deployed systems. `SoftwareSerial` is convenient for
quick tests, but it has these limits:

- Only one `SoftwareSerial` instance can listen at a time.
- The usable RX pins depend on the board.
- It restricts interrupts while it sends or receives. This can disturb Servo,
  timer-based code, other software UARTs, and similar features.
- It can drop received bytes when the main loop is heavily loaded. Evaluate it
  under that load.

Do not access one UART from multiple tasks or interrupt contexts at the same
time. Connect only one sensor to each UART.

If you use a custom `Stream`, implement current Arduino `flush()` semantics:
wait until pending transmit data has been sent, or return immediately if writes
are synchronous. `flush()` must not discard received bytes. If another context
reads a byte between `available()` and `read()`, so that `read()` returns `-1`,
the timeout must still advance.

Plan for the case where only one side loses power. If the Arduino keeps driving
TX after the sensor loses power, the signal line can back-power the sensor.
While the sensor is unpowered, make the Arduino TX high impedance, isolate the
signal with a converter that has powered-off protection, or use an equivalent
measure that follows the product documentation. Cover restart, maintenance,
connector failure, and supply failure in the design.

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

`readMeasurement()` starts a conversion and waits at least 300 ms after the start
ACK. It then polls the state no more often than every 200 ms. After the default
30-second conversion deadline, counted from the start ACK, it starts no new
polls. If the application must keep doing other work during the wait, use the
cooperative API.

The cooperative API uses this state sequence:

```text
IDLE --startMeasurement()--> MEASURING
MEASURING --measurementComplete(false)--> MEASURING
MEASURING --measurementComplete(true)--> READY
READY --readMeasurementData()--> IDLE
```

For the first 300 ms after the start ACK, `measurementComplete()` returns
`complete=false` without sending a UART request. After that, a call made before
the configured poll interval has passed since the previous poll also returns
without a request. You can therefore call it repeatedly from the main loop. See
[`UnoCooperativePolling`](../examples/UnoCooperativePolling/UnoCooperativePolling.ino)
for a complete example.

The default 30-second deadline is an upper limit for the measurement wait, not
the normal conversion time. The cooperative API splits up the conversion wait,
but not the UART response waits. `startMeasurement()`, a `measurementComplete()`
call that sends a status poll, and the final 18-byte `readMeasurementData()`
each perform one UART transaction. The caller is blocked while the driver waits
for the response, for up to one response timeout (about 300 ms with defaults).
If a status poll and the data read run back-to-back, budget for both waits. For
tighter latency, move UART communication into your own asynchronous state
machine, an RTOS task, or a dedicated communication routine.

Calling `readMeasurementData()` out of order fails with `MEASUREMENT_NOT_READY`.

## Timeouts and recovery

```cpp
sensor.setResponseTimeout(300);
sensor.setPollInterval(200);
sensor.setMeasurementTimeout(30000);
```

These defaults are the reference values for this sample. Shorter values can
treat normal delays as errors. Longer values delay fault detection. If you
change them, evaluate the new values with the actual board, UART, wiring,
sensor, and temperature range. Timeout calculations use unsigned subtraction,
so they keep working when `millis()` rolls over.

The measurement timeout is the conversion-wait deadline: after the start ACK, a
new status poll can begin only before this deadline. It is not a limit on the
total time of an API call. A UART response wait or a blocking poll-interval
delay that begins before the deadline is not cut off at the deadline, so the
call can run past it. If your system has a strict processing deadline, add
margin for each response timeout or manage the deadline at a higher level.

The APIs that communicate with the sensor return `true` on success and `false`
on failure. On failure, record
`lastError()`, `lastDeviceErrorCode()` for a device error, and a timestamp. Do
not resend requests rapidly within one cycle. Discard the measurement state and
retry from `startMeasurement()` in a later cycle. If errors persist, check the
power supply, ground, UART signal levels, wiring, and noise. Turn off the power
before you change any wiring.

If the sensor's internal measurement fails after the start ACK, a separate error
frame can arrive later. Treat a following device error, format error, or
measurement timeout as a failed measurement. Before each request, the driver
discards stale input, so it resynchronizes after a delayed error frame or a
partial frame.

## Firmware capability and measurement selection

Read the sensor information at startup and keep the capability result:

```cpp
SLT5006::SensorInfo info;
bool hasCocoPoreEc = false;
if (sensor.readInfo(info)) {
  hasCocoPoreEc = SLT5006::supportsEcPoreCoco(info);
}
```

Firmware 1.6.3 does not support `ecPoreCocoDsM`. Firmware 1.7.5 and 1.7.6
support it. Unsupported firmware still returns word 9, but its value is zero
and is not a supported measurement. If the firmware does not support the field
or cannot be identified, do not display, store, or upload the value. Evaluate
the other measurements against the product specifications.

Judge communication success and measurement validity separately:

1. Confirm the communication path: read the firmware version and serial number,
   and check that the responses have valid CRCs.
2. Select the one VWC field that matches the actual medium: `vwcPercent` for
   general soil, `vwcRockPercent` for rock wool, or `vwcCocoPercent` for coco
   peat.
3. Distinguish bulk EC from pore-water EC according to the quantity you need.
4. Before using coco peat pore EC (`ecPoreCocoDsM`), check both firmware support
   and the application conditions in the product documentation.
5. Install the sensor following Murata's [recommended installation](https://www.murata.com/en-global/products/sensor/soil/design-development/institution)
   and check the response and repeatability in a known medium and installation.
6. Define how the application handles out-of-range values, saturation, and
   abnormal values. The driver does not clamp values to the specification range.

Do not store or average the medium-specific conversions as if they were
independent simultaneous measurements.

## Memory and porting checks

The driver does not use dynamic `String` objects, heap allocation, or
exceptions. It receives responses into a fixed 31-byte buffer, sized for the
longest response, and returns converted `float` values. Units are dS/m for EC,
percent for VWC, degC for temperature, and V for voltage. We recommend including
the unit in stored column names. On the Arduino Uno, `double` has the same
32-bit precision as `float`. If you need higher precision, evaluate the
requirement together with the target board and data format.

When you port the protocol, read the [protocol guide](protocol.en.md) and test
these points in particular:

- CRC value and high-byte-first wire order
- Fragmented responses
- Four-byte error frames
- Every response-header and echo check
- Signed 12-bit temperature and little-endian values
- The 300 ms settle time before the first status request
- The 30-second deadline and `millis()` rollover
