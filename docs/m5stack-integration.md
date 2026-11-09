# System integration guide

English | [日本語](m5stack-integration.ja.md)

Use `M5Core2QuickStart` for the first functional check and `M5Core2CloudLogger`
to evaluate storage and upload. In a product, keep UI, storage, communication,
and authentication separate, with clear boundaries:

```text
SLT5006 UART driver -> Measurement -> display / local CSV / upload queue -> GAS
```

## Arduino compatibility

The M5Core2 driver uses the same `Stream`-based API and data types as the
Arduino version. Configure ESP32 hardware UART2 and pass it to the driver. This
example uses PORT.A on the Core2 (RX=32, TX=33). For PORT.C on a compatible
Bottom, use RX=13, TX=14:

```cpp
HardwareSerial sensorSerial(2);
SLT5006 sensor(sensorSerial);

void setup() {
  sensorSerial.begin(SLT5006::BAUD_RATE, SERIAL_8N1, 32, 33);
}
```

Use `readMeasurement()` for a simple blocking measurement. To keep the UI and
network running during the conversion, call `startMeasurement()`, call
`measurementComplete()` repeatedly, and then call `readMeasurementData()`. These
calls split up the conversion wait, but each UART transaction still blocks the
caller while it waits for the response, for up to the response timeout.

At storage and API boundaries, keep the field names and units compatible with
the Arduino and Python versions: `battery_v` is the supply voltage reported by
the sensor, `core2_battery_v` is the Core2 battery voltage, temperature is in
degC, VWC is in percent, and EC is in dS/m. Select the one VWC conversion that
matches the medium. Judge measurement validity separately from communication
success. Define the installation method, the specification range, and the
handling of saturation and abnormal values as system requirements. The driver
does not clamp values to the specification range.

Read the firmware version and serial number at startup. Firmware older than
1.7.5 still returns word 9, but its value is zero and is not a supported coco
peat pore EC measurement. Omit the field when the firmware is older than 1.7.5
or cannot be identified. See the [protocol reference](m5stack-protocol.md) for
frames, conversions, and error codes.

## Errors, deadlines, and ownership

The defaults are a 300-ms UART response timeout, a 200-ms state-poll interval,
and a 30-second conversion deadline. The first poll comes at least 300 ms after
the start ACK. Shorter values can flag normal delays as faults, and longer values
delay detection of a broken wire. If you change them, test with the actual
sensor, wiring, temperature, and processing load.

On an error, log the error class, device error code, time, and number of
consecutive failures. Do not flood the sensor with the same request. Clear stale
input, wait, and retry in a new measurement cycle. After CRC or format errors,
check the power supply, GND, logic levels, noise, and cable length. Apply
exponential backoff with an upper limit so that repeated failures do not cause a
watchdog reset loop. Turn off all power before changing the wiring.

Use unsigned subtraction to compute elapsed `millis()` time so that wraparound
is handled. For a hard application deadline, also budget the UART response
timeouts of the start, state-poll, and final read transactions. Give one task
sole ownership of the UART. Never access it from two tasks or interrupt
contexts.

## Store before upload

For every measurement:

1. Build a record with named fields and a unique `id` that stays the same across
   retries.
2. Write the record to a temporary file, then rename it into the queue.
3. Append the same record to the local CSV file, then flush and close the file.
4. POST the record. After redirects, require both HTTP 2xx and JSON `ok:true`
   in the final response.
5. Delete the queued record only after that confirmation.

Check the queue write and the CSV write separately. The queue takes priority
because it preserves cloud delivery. A header or row write succeeds only when it
writes the complete byte count, not just a nonzero count. Queued JSON records
are limited to 2048 bytes, and an existing ID is never overwritten. Even with a
temporary file and rename, power loss can still lose or corrupt data on some
cards. Plan for a reset during each step, and detect and isolate incomplete
files at startup. Also plan for full or removed media, card wear, retention,
backup, an unsynchronized clock, escaping of external input (including CSV
injection), and queue limits.

Use the same `id` to match local and cloud data. Build it from the time plus a
persistent device identity or counter. A timestamp alone can collide, or can
change after clock correction.

Write `ts` in ISO 8601 with a UTC offset or `Z`. Do not record the time before
NTP synchronization as a valid date and time. Until NTP is synchronized, the
sample logger stores an empty `ts`, `error=time_not_synchronized`, and a stable
`id`. Only in that case does the included GAS use the server receipt time for
the sheet Date. That value is not the measurement time. Products that need
accurate chronology should add RTC time retention, a synchronization-quality
flag, boot time with monotonic offsets, and a policy for clock jumps after
resynchronization.

## Network, credentials, and power

- Accept HTTPS URLs only, and validate the certificate and hostname.
- Follow GAS redirects safely to the final response, and validate its JSON
  schema and `ok:true`.
- Keep Wi-Fi passwords and tokens out of source code, the UI, logs, and crash
  dumps.
- Set limits on timeouts, retry count, backoff, and queue size. Document how to
  replace the token if it leaks.
- For a fleet of devices, replace the shared token with per-device credentials,
  revocation, rate limits, and audit logging.

Deep sleep and sensor power control need a circuit review, not only software
changes. The circuit must prevent back-powering through the UART when either
device is off. If you control Enable from a GPIO, check the pin ratings, the pin
state at boot, the driver circuit, and the power-off order after a fault, and
wait for the supply to stabilize. Never sleep or cut power during an SD write or
an HTTPS transaction.

## Verification checklist

- Wiring: GPIO assignment, signal direction, supply margin, and every case where
  only one side is powered.
- UART: known frames, split and stale responses, timeouts, CRC, and maximum
  length.
- Protocol: error frames, echo mismatch, signed temperature, and the firmware
  version boundary.
- Scheduling: collisions between manual and periodic measurements, `millis()`
  wraparound, and Wi-Fi delays.
- Storage: the first header write, CSV quoting, full, removed, or corrupted
  media, and a reset during a write.
- Upload: recovery from offline, redirects, TLS failure, 401-equivalent errors,
  `ok:false`, and duplicate retries.
- Time: no NTP, time zone and offset, resynchronization, and wake from deep
  sleep.
- Endurance: continuous operation, RSSI, temperature, power margin, and wet/dry
  changes at the intended installation.

Do not hot-plug the sensor wiring during testing. Inject hardware faults only
through switches or fixtures built for that purpose.
