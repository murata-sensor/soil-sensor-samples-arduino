# System integration guide

English | [日本語](m5stack-integration.ja.md)

Use `M5Core2QuickStart` for first verification and `M5Core2CloudLogger` to evaluate
storage and upload. In a product, preserve this separation:

```text
SLT5006 UART driver -> Measurement -> display / local CSV / upload queue -> GAS
```

## Arduino compatibility

The M5Core2 driver retains the Arduino sample's `Stream`-based API and data
types. Pass a configured ESP32 hardware UART. This example uses built-in
PORT.A (RX=32, TX=33); use RX=13, TX=14 for PORT.C on a compatible bottom:

```cpp
HardwareSerial sensorSerial(2);
SLT5006 sensor(sensorSerial);

void setup() {
  sensorSerial.begin(SLT5006::BAUD_RATE, SERIAL_8N1, 32, 33);
}
```

Use `readMeasurement()` for a simple blocking operation. For a responsive UI,
call `startMeasurement()`, repeatedly call `measurementComplete()`, then call
`readMeasurementData()`. This cooperates during conversion but individual UART
transactions can still block for their response timeout. Give one task sole
ownership of the UART.

Keep the Arduino/Python-compatible field names and units at storage/API
boundaries: `battery_v` is the sensor-reported supply, `core2_battery_v` is the
Core2 battery, temperature is degC, VWC is percent, and EC is dS/m. Select one
VWC conversion appropriate to the medium. Communication success does not prove
measurement validity, and the driver does not clamp values to specification.

Read firmware and serial at startup. Firmware older than 1.7.5 returns a
readable zero for word 9, but that zero is not a supported coconut pore EC
measurement. Omit the field when firmware is older than 1.7.5 or unidentified. See
the [protocol reference](m5stack-protocol.md) for frames, conversions, and error codes.

## Errors, deadlines, and ownership

Defaults are a 300-ms UART response timeout, 200-ms state-poll interval, and
30-second conversion deadline, with at least 300 ms before the first poll. Test
any change on the real sensor and wiring. Log error class, device code, time,
and consecutive count; clear stale input, wait, and retry from a new cycle
instead of flooding the current request. Apply bounded backoff.

Use unsigned elapsed-time arithmetic for `millis()` wraparound. A hard
application deadline must also budget the start, state, and final UART response
timeouts. Never access the same UART from two tasks or interrupt contexts.

## Store before upload

For every measurement:

1. Build a named record with an ID that remains stable across retries.
2. Persist it to a queue with temp-file plus rename.
3. Append and flush/close the same record in local CSV.
4. POST it and require both final HTTP 2xx and JSON `ok:true` after redirects.
5. Delete the queued record only after that confirmation.

Queue and CSV writes are separately checked; the queue has priority because it
preserves delivery. Header and row writes must report the complete byte count,
not merely a nonzero write. Queued JSON records are limited to 2048 bytes and
existing IDs are never overwritten. Temp-file plus rename does not by itself
guarantee power-loss durability on every card. Plan for reset during each step, full or removed media,
card wear, retention, backup, malformed CSV input, and queue limits. Use the
same `id` to correlate
local and cloud data. Combine persistent device identity/counter with time;
timestamp alone can collide or move after clock correction.

Use ISO 8601 with an offset or `Z`. The sample stores empty `ts` plus
`error=time_not_synchronized` before NTP is valid. The included GAS then uses
server receipt time for the sheet Date, which is not measurement time. Products
that need precise chronology should add retained RTC, synchronization quality,
monotonic offsets, and explicit clock-jump handling.

## Network, credentials, and power

- Accept HTTPS only and validate certificate plus hostname.
- Follow GAS redirects safely and validate the final JSON schema.
- Keep Wi-Fi passwords and tokens out of source, UI, logs, and crash dumps.
- Bound timeouts, retry count, backoff, and queue size; document token rotation.
- For fleets, replace one shared token with per-device credentials, revocation,
  rate limits, and audit logging.

Deep sleep and sensor power control require a circuit-level review. Prevent
UART back-powering when either device is off, validate Enable ratings and boot
pin state, wait for power stabilization, and never sleep during an SD write or
HTTPS transaction.

## Verification checklist

- Wiring: GPIO and signal direction, supply margins, all one-sided power cases.
- UART: known frames, split/stale responses, timeout, CRC and maximum length.
- Protocol: error frames, bad echo, signed temperature, firmware boundary.
- Scheduling: manual/periodic collision, wraparound, Wi-Fi delays.
- Storage: header, CSV quoting, full/removal/corruption, reset mid-write.
- Upload: offline recovery, redirects, TLS failure, `ok:false`, duplicate retry.
- Time: no NTP, zone/offset, resynchronization, deep-sleep boot.
- Endurance: continuous operation, RSSI, temperature, power, and wet/dry change
  at the intended installation.

Inject hardware faults only through approved switches or fixtures; do not
hot-plug the sensor wiring during testing.
