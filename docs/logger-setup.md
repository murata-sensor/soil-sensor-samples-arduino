# M5Core2CloudLogger setup

English | [日本語](logger-setup.ja.md)

`M5Core2CloudLogger` adds CSV logging, Wi-Fi/NTP, HTTPS upload to Google Sheets,
and a retry queue to the SLT5006 display. Read the [wiring guide](m5stack-wiring.md) and
verify the sensor with `M5Core2QuickStart` first.

## microSD and USB/NVS configuration

With the Core2 off, insert a backed-up compatible microSD card for CSV and pending
records.
Confirm that USB transfer, internal NVS storage, and device-originated SD writes
are permitted by your organization.

### Configuration priority

The logger uses the complete NVS configuration if present. Only when NVS is
unconfigured does it read `/slt5006.cfg` from the microSD root. Without either,
offline 60-second defaults apply. Fields are not merged. Saved offline defaults
(including a USB upload of `{}`) still take precedence over SD. NVS read errors
or corruption are reported and use offline defaults, not SD fallback.
SD configuration is not automatically copied into NVS.

### Configure using microSD

For an unconfigured NVS, place a UTF-8 `/slt5006.cfg` on the card:

```ini
wifi_ssid=YOUR_WIFI_SSID
wifi_password=YOUR_WIFI_PASSWORD
gas_url=https://script.google.com/macros/s/REPLACE_WITH_DEPLOYMENT_ID/exec
gas_token=REPLACE_WITH_RANDOM_TOKEN
sample_interval_sec=60
upload_batch_size=10
ntp_server=pool.ntp.org
timezone=JST-9
deep_sleep_sec=0
```

Restart after editing. Maximum file size is 4096 bytes, with LF or CRLF.
Blank lines and lines beginning with `#` or `;` are ignored. Outer whitespace
is trimmed; matching single/double quotes preserve inner whitespace. The last
duplicate key wins. Unknown keys, missing `=`, invalid/out-of-range integers and
NUL are errors. String limits and cloud requirements match USB configuration.
Invalid files are not partially applied; errors are reported and offline defaults
are used. SD credentials are not encrypted: restrict physical access to the card.
Use USB provisioning when PC writes to removable media are restricted.

### Configure using USB/NVS

Configuration is stored in internal NVS, a separate flash partition from the
application. Power-off and normal firmware Upload preserve it; full flash erase,
NVS erase, or partition changes do not guarantee preservation. NVS is not
encrypted by this sample: restrict physical access to the Core2.

Flash the logger firmware, connect USB, and close Serial Monitor. On Windows:

```powershell
py -m pip install pyserial
py .\scripts\configure_logger.py --port COM4
```

Enter credentials and settings interactively; password, URL, and token input
is hidden. Press Enter to accept defaults. No PC config file is created.
Once the tool is waiting, press Core2 reset if it is sleeping or busy. Each boot
has a three-second USB provisioning window before sensor and Wi-Fi waits.
`NVS configuration saved` confirms success; Core2 restarts automatically.
Provisioning works without SD and requires no firmware rebuild.

Alternatively, send a private JSON file from the PC's local disk:

```powershell
py .\scripts\configure_logger.py --port COM4 --file .\slt5006-config.json
```

```json
{
  "wifi_ssid": "YOUR_WIFI_SSID",
  "wifi_password": "YOUR_WIFI_PASSWORD",
  "gas_url": "https://script.google.com/macros/s/REPLACE_WITH_DEPLOYMENT_ID/exec",
  "gas_token": "REPLACE_WITH_RANDOM_TOKEN",
  "sample_interval_sec": 60,
  "upload_batch_size": 10,
  "ntp_server": "pool.ntp.org",
  "timezone": "JST-9",
  "deep_sleep_sec": 0
}
```

| Key | Default / range | Purpose |
|---|---|---|
| `wifi_ssid` / `wifi_password` | empty | Wi-Fi credentials; empty SSID disables Wi-Fi |
| `gas_url` / `gas_token` | empty | HTTPS `/exec` URL and matching `INGEST_TOKEN` |
| `sample_interval_sec` | 60; 5-86400 | Measurement start interval while awake |
| `upload_batch_size` | 10; 1-20 | Maximum queued rows per POST |
| `ntp_server` | `pool.ntp.org` | NTP host |
| `timezone` | `JST-9` | ESP32 POSIX time-zone string |
| `deep_sleep_sec` | 0 or 10-86400 | Sleep duration after a scheduled cycle |

`timezone` uses POSIX syntax, not an IANA name; test offsets and daylight-saving
rules for non-Japan locations. Logger JSON/CSV `ts` stays in UTC `Z` form. GAS
`TIME_ZONE` is separately an IANA Sheet display zone such as `Asia/Tokyo`.

Every upload replaces the entire config; omitted keys revert to defaults.
Send `{}` to restore offline defaults for this logger without erasing other NVS
data. Unknown keys, wrong types, and out-of-range integers are rejected, not
clamped. String whitespace is preserved. Limits in UTF-8 bytes: SSID 32,
password 64, URL 512, token 256, NTP host 253, timezone 128. Embedded NUL and
empty NTP/timezone strings are rejected. Cloud requires SSID, HTTPS URL and token;
to disable cloud, leave both URL and token empty. The PC tool rejects duplicates.

The USB protocol uses 115200 baud: send `CONFIG BEGIN` plus newline, wait for
`[CONFIG] READY`, then send one UTF-8 JSON object line (maximum 2048 bytes,
excluding newline; LF or CRLF). Only successful validation and NVS commit return
`[CONFIG] SAVED`, followed by restart. Failures return `[CONFIG] ERROR ...`
without echoing values. Send `CONFIG CANCEL` to abort. Provisioning pauses
measurement, upload and sleep transitions, expires after 120 seconds, and drops
incomplete lines after 10 seconds. Invalid input is not saved; cancellation makes
no new write. A lost save acknowledgement may mean settings were already saved:
check the device before retrying. A paused measurement may time out after cancellation; reset while the
tool is waiting to provision before measurement instead.

Never commit the real config, Wi-Fi password, GAS token, or deployment URL.
`slt5006-config.json` is gitignored, but other filenames are not. Rotate credentials
after device loss or exposure. Errors/timeouts are not save confirmations.

Without microSD, sensing, display, USB measurement output, Wi-Fi and NTP use the
NVS settings. CSV, retry queue, and cloud upload are unavailable: there is no
direct-upload fallback. Without NVS config, offline 60-second defaults apply.

## Controls and storage

The display aligns labels, right-aligned numeric values, and units in fixed
columns. All media show Temp before VWC, followed by EC bulk.
Sensor serial number and firmware appear below the battery strip, or
`unavailable` when sensor information could not be read.
After a sensor failure, retained last-good values are marked `(STALE)` until
the next successful measurement, including after an upload succeeds.
Measurement timing (60 seconds by default) is separate from the
0.5-second screen refresh. Frames are drawn off-screen and transferred together
to avoid black-screen flicker. Completed cycles leave an idle loop for uploads,
even when conversion takes longer than `sample_interval_sec`, so repeated
long measurements or timeouts do not starve the queue.
Buffer allocation failure is reported and uses
direct drawing instead. `SLT5006 supply` is the sensor-reported supply voltage. The separate
shaded `Core2 battery (est.)` strip shows the Core2 battery level estimate and
voltage, refreshed every five seconds while the main loop is running, even
before a sensor measurement succeeds. The percentage is only a guide and can
vary with load and charging. Unavailable readings are shown as `unavailable`
and reported in Serial Monitor. The CSV/cloud record format is unchanged;
the estimated percentage is display-only.

| Touch button | Action |
|---|---|
| A | Manual measurement request (waits if a measurement is already running) |
| B | Cycle display medium: General / Rock wool / Coco |
| C | Reconnect Wi-Fi and upload queued records |

Button B selects the applicable conversion; it does not create three independent
moisture measurements. Select fields based on the medium and product guidance.
The Rock wool view hides the `EC pore` row.
The Coco view shows `--` for pore EC when firmware is unsupported or unknown;
general-soil pore EC is never substituted for the unavailable coco value.
This selection affects only the display, not CSV storage or cloud uploads.
All three VWC conversions and the EC fields are recorded regardless of the
selected view; coconut pore EC is sent only when the sensor firmware supports
it. Core2 battery percentage is not sent, while its voltage remains recorded.
The cloud destination is the `gas_url` HTTPS `/exec` URL in the selected NVS or
SD configuration; `gas_token` must match GAS `INGEST_TOKEN`. USB saving restarts
the Core2; restart manually after editing SD configuration.
No rebuild is needed.

Successful measurements and sensor communication errors append to
`/slt5006.csv`. Error rows leave measurement values empty and identify the
failure in `error`; normal rows include the ISO timestamp, sensor serial and
firmware, sensor-reported `battery_v`, Core2 `core2_battery_v`, temperature,
three VWC conversions, EC fields, and RSSI. Unsupported coconut pore EC remains
empty rather than becoming a false zero.
Unavailable Core2 battery voltage is null in JSON and empty in CSV, not a
zero or negative reading. RSSI `-127` denotes disconnected Wi-Fi.
CSV header and row writes are checked for the complete byte count. Partial
writes are reported as failures; a successfully queued record remains available
for cloud delivery. After an SD write error, stop the Core2 and inspect/back up
the CSV before resuming, because a partial line may remain in the file.

Pending records are individual files under `/slt5006_queue`.
Before each batch, the logger scans the entire queue and selects the oldest UTC
`ts` first. Empty timestamps are sent before dated records; ties (including empty
timestamps) use filename order. It keeps only the configured batch size (default
10, maximum 20) in memory; scan time grows with queue size. As successful batches
are deleted, a static backlog drains oldest-first. CSV storage and acknowledgement
requirements are unchanged.

Each queued JSON file is limited to 2048 bytes. Existing record IDs are not
overwritten. Oversized, partially read, NUL-containing, or malformed JSON files
(including trailing non-JSON data) are retained, excluded from upload batches,
and reported on USB Serial. An unreadable queue directory, or a queue containing
only rejected records, reports failure rather than an empty-queue success.
Each valid record retains a
stable `id` across retries. A file is removed only after HTTPS 2xx **and** final
GAS JSON `ok:true`; ambiguous responses stay queued. GAS deduplicates the same
ID if a write succeeded but the response was lost. Do not manually edit queue
files. Back up CSV and monitor queue-file count and free card space on the card.
GAS result redirects (302/303) are fetched using GET on a fresh connection.
`[UPLOAD] hop=... method=... status=...` identifies whether the original POST
or a result GET failed. Non-HTTPS redirect destinations are rejected.
Each request explicitly closes its non-reusable TLS connection before HTTP
cleanup, avoiding an unnecessary receive probe when discarding a redirect body.
This does not suppress errors during POST, GET, or response-body reads.
The screen is redrawn before blocking work: `Connecting Wi-Fi...`,
`Preparing upload...`, `Uploading ... row(s)...`, and
`Fetching upload result...` are separate from `Measuring...`.
Uploads remain synchronous; buttons are not serviced during HTTPS requests.
`[UPLOAD] accepted_rows=... queue_cleaned=1` confirms both the successful
response check and removal of the acknowledged batch from the SD queue.

Queued sensor failures are uploaded too. A row with serial number 0, empty
firmware/measurements and `sensor response timed out` records a failed attempt
before sensor information was available. Distinguish it from current successful
measurements by its ID and timestamp.

### Upload connection errors

Wi-Fi `online` means access-point connectivity, not successful access to GAS.
Negative upload results (such as -1) are transport errors, not server HTTP status
codes. Open Serial Monitor at 115200 baud and press C to retry. The `[UPLOAD]`
line reports the transport description, TLS code/description, Wi-Fi status, RSSI
and clock validity, without URL, token or payload. For `clock_valid=0`, check
NTP; for TLS errors, check certificate validation, time and the network path.
A zero TLS code does not rule out DNS/TCP failures. Keep certificate validation
enabled and use diagnostics to identify the cause. Failed rows remain queued.
The embedded Google trust anchors are GTS Root R1 and R4. TLS -9984 means
certificate verification failed, which can reflect missing trust roots, an
incorrect clock, or certificate replacement on the network path.
`clock_valid=1` only checks that the year is 2024 or later: compare the UTC `ts`
in USB measurement JSON with the actual time as well.

## Deep sleep

`deep_sleep_sec=0` keeps the logger awake. At 10 or more, each boot immediately
attempts one measurement, storage, and upload, then sleeps even after a sensor
failure. The display, Wi-Fi, buttons, and sensor loop do not run during sleep.
This mode ignores `sample_interval_sec`; boot, sensor stabilization, Wi-Fi, and
NTP make the effective start-to-start interval longer than the sleep duration.

Before battery deployment, test power behavior and UART back-powering during
sleep, real start-to-start timing, NTP failure, SD write recovery, and current
consumption on the complete hardware.

## Acceptance check

1. Verify firmware, serial, and measurements with `M5Core2QuickStart`.
2. Boot without NVS or SD config; check offline operation and USB setup guidance.
3. Provision over USB; verify the UTC `ts` in Serial/CSV and one `/slt5006.csv` row.
4. Disable Wi-Fi, measure, and confirm CSV plus queued data remain available.
5. Restore Wi-Fi, press C, and confirm one sheet row and a reduced queue.
6. With power off, disconnect the sensor; verify timeout reporting and recovery
   after reconnecting safely.
7. Test continuous operation, RSSI, supply margin, card capacity, and wet/dry
   response at the actual installation.
8. Power-cycle and perform a normal firmware Upload; confirm settings persist.
   Test provisioning without SD, invalid input, cancellation and deep-sleep reset.
9. Verify SD configuration with unconfigured NVS, then NVS priority after USB saving.
10. Check unsupported/unknown Coco EC shows `--`, last-good values are marked
    `(STALE)` after a sensor failure, and SD failures do not discard queued rows.
11. With a five-second interval and an approved slow-sensor fixture, check
    uploads/reconnects still run between long measurements or timeouts.

See [Google Apps Script setup](gas-setup.md) for cloud deployment and the
[integration guide](m5stack-integration.md) before incorporating this sample into a
larger system.
