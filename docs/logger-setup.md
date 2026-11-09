# M5Core2CloudLogger setup

English | [日本語](logger-setup.ja.md)

`M5Core2CloudLogger` adds CSV logging to microSD, Wi-Fi/NTP, HTTPS upload to
Google Sheets, and a retry queue for failed uploads to the SLT5006 measurement
display. Read the [wiring guide](m5stack-wiring.md) first, and check the sensor
information and measurements with `M5Core2QuickStart` before you use the logger.

## microSD and USB/NVS configuration

The microSD card stores the measurement CSV and the queue of pending records.
Check the Core2 and library documentation for supported file systems and card
capacities. Back up any data you need from the card, then insert it with the
Core2 powered off.

Before use, confirm that your organization permits USB configuration transfer,
storage in the internal NVS, and log writes from the Core2 to the microSD card.

### Configuration priority

If NVS holds a saved configuration, the logger uses that configuration as a
whole. Only when NVS is unconfigured does the logger read `/slt5006.cfg` from the
microSD root. If neither exists, it runs with the offline defaults and a
60-second interval. The logger never merges fields from NVS and SD. A saved
offline configuration in NVS, including one created by sending `{}` over USB,
still takes precedence over SD. An NVS read error or corrupted NVS data does not
count as unconfigured: the logger shows the error on the screen and USB Serial
and runs with the offline defaults, without falling back to SD. The logger does
not copy the SD configuration into NVS.

### Configure using microSD

If NVS is unconfigured, place a UTF-8 file named `/slt5006.cfg` in the root of
the card:

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

Restart the Core2 after editing the file. The file can be up to 4096 bytes and
can use LF or CRLF line endings. Blank lines and lines that begin with `#` or `;`
are ignored. Whitespace around keys and values is trimmed. To keep whitespace
inside a value, enclose the whole value in matching single or double quotes. If
a key appears more than once, the last value is used. Unknown keys, lines without
`=`, invalid or out-of-range integers, and NUL characters are errors. String
limits and cloud requirements are the same as for USB configuration. If
validation fails, the logger applies none of the file, reports the error, and
runs with the offline defaults. Credentials on the card are not encrypted, so
restrict physical access to the card. If PC writes to removable media are
restricted, use USB configuration instead.

### Configure using USB/NVS

The logger stores Wi-Fi, cloud, measurement-interval, and other settings in NVS,
a partition of the Core2's internal flash that is separate from the application.
The settings survive power-off and normal firmware uploads. Erasing the entire
flash or NVS, or changing the partition layout, can erase the settings. This
sample does not encrypt NVS, so restrict physical access to the Core2.

Flash the logger firmware to the Core2, connect the Core2 to the PC over USB, and
close the Serial Monitor in VS Code or Arduino. Then run the following on
Windows, replacing `COM4` with your port:

```powershell
py -m pip install pyserial
py .\scripts\configure_logger.py --port COM4
```

The tool prompts for the Wi-Fi SSID and password, the GAS URL and token, and the
other settings. Password, URL, and token input is hidden. Press Enter to accept a
default value. The tool sends the settings over USB and does not save them to a
file on the PC. When the tool starts waiting for the Core2, press the Core2 reset
button if the Core2 is sleeping or busy. After each boot, the Core2 accepts USB
configuration for three seconds before it starts sensor and Wi-Fi processing.

`NVS configuration saved` confirms success. The Core2 then restarts
automatically and runs with the NVS configuration. USB configuration works
without a microSD card and needs no firmware rebuild. An error, or a timeout
while waiting for the acknowledgement, is not a save confirmation. In that case,
check the port, the input values, and the device.

Instead of answering the prompts, you can send a private JSON file stored on the
PC's local disk:

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
| `wifi_ssid` / `wifi_password` | empty | Wi-Fi SSID and password. An empty SSID disables Wi-Fi |
| `gas_url` / `gas_token` | empty | GAS HTTPS `/exec` URL and the token that matches `INGEST_TOKEN`. Leave both empty to disable uploads |
| `sample_interval_sec` | 60; 5-86400 | Interval between measurement starts in continuous operation (seconds) |
| `upload_batch_size` | 10; 1-20 | Maximum queued rows per POST |
| `ntp_server` | `pool.ntp.org` | NTP server name |
| `timezone` | `JST-9` | ESP32 POSIX time-zone string |
| `deep_sleep_sec` | 0 or 10-86400 | 0 = continuous operation; 10 or more = deep-sleep time after each cycle (seconds) |

`timezone` takes an ESP32/POSIX string, not an IANA name. Japan uses `JST-9`.
For other locations, keep in mind that the POSIX offset sign is the reverse of
the usual UTC notation, and check the daylight-saving rules. The logger always
writes `ts` in JSON and CSV as UTC in `Z` form, regardless of location. The GAS
`TIME_ZONE` setting uses a different format: an IANA name, such as `Asia/Tokyo`,
for display in the Sheet.

Each USB configuration replaces the entire configuration, and omitted keys
revert to their defaults. Send `{}` to restore this logger's offline defaults;
other NVS data is not erased. Unknown keys, wrong types, and out-of-range
integers are rejected, not clamped, and nothing is saved. Specify numbers as JSON
integers, not as strings, decimals, or booleans. Whitespace in strings, including
the password, is saved as entered. String limits in UTF-8 bytes are: SSID 32,
password 64, URL 512, token 256, NTP server 253, and timezone 128. NUL characters
and an empty NTP server or timezone are rejected. To use the cloud, set the SSID,
an HTTPS URL, and the token. To disable the cloud, leave both the URL and the
token empty. The PC tool also rejects duplicate keys.

The USB protocol runs at 115200 baud. Send `CONFIG BEGIN` and a newline, wait for
`[CONFIG] READY`, and then send the UTF-8 JSON object as one line followed by a
newline. The line can be up to 2048 bytes, excluding the newline, and can end
with LF or CRLF. The Core2 returns `[CONFIG] SAVED` and restarts only after both
validation and the NVS write succeed. On failure, it returns
`[CONFIG] ERROR ...`. Responses do not include setting values or credentials.

Send `CONFIG CANCEL` to abort. Configuration mode pauses measurement, uploads,
and the transition to sleep. It ends after 120 seconds, and a partly received
line is dropped after 10 seconds. Invalid input is not saved, and cancelling
does not write anything new.

If the save acknowledgement is lost, the settings may already be saved; check
the device before retrying. If a measurement is in progress when configuration
mode starts, that measurement can time out after cancellation. Normally, reset
the Core2 while the tool is waiting, so that configuration finishes before any
measurement starts.

Never commit the real configuration file, Wi-Fi password, GAS token, or
deployment URL. `slt5006-config.json` is excluded by `.gitignore`, but files
saved under other names are not. If the device is lost or a credential is
exposed, change the credentials.

A microSD card is not required for measurement and display. Without a card, or
if card initialization fails, the logger still measures at the NVS interval and
shows the results on the screen and USB Serial. Wi-Fi and NTP also follow the
NVS settings. CSV logging, the retry queue, and cloud uploads are unavailable,
because uploads go through the SD queue and there is no direct upload. Without
an NVS configuration, the offline defaults with a 60-second interval apply.
Clear the SD error on the screen before you start logging or cloud uploads.

## Controls and storage

The display shows labels, right-aligned numeric values, and units in fixed
columns. For every medium, Temp comes first, then VWC, then EC bulk. The sensor
serial number and firmware version appear below the battery strip, or
`unavailable` if the sensor information could not be read.

After a sensor failure, the retained last-good values are marked `(STALE)`. The
mark stays even after a successful upload and clears at the next successful
measurement.

The measurement interval (60 seconds by default) is independent of the
0.5-second screen refresh. Each frame is drawn off-screen and transferred at
once, which avoids black-screen flicker. If the frame buffer cannot be
allocated, the logger reports an error and draws directly to the screen instead.
Even when a measurement takes longer than `sample_interval_sec`, the logger runs
one loop pass for uploads and reconnection after each measurement, before it
starts the next one. Repeated long measurements or timeouts therefore do not
starve uploads and reconnection.

`SLT5006 supply` is the supply voltage reported by the sensor. The separately
shaded `Core2 battery (est.)` strip shows the estimated Core2 battery level and
voltage. The strip appears even before a sensor measurement succeeds, and the
reading is refreshed every five seconds while the main loop is running. The
percentage is an estimate that varies with load and charging. If a reading is
unavailable, the strip shows `unavailable` and the logger also reports it in
Serial Monitor. The percentage is display-only and is not added to CSV or cloud
records.

| Touch button | Action |
|---|---|
| A | Request a manual measurement (waits if a measurement is already running) |
| B | Cycle the displayed medium: General / Rock wool / Coco |
| C | Reconnect Wi-Fi and upload queued records |

Button B only selects which conversion is displayed. The three VWC values are
conversions of the same measurement for different media. Use `vwc_pct` for
general soil, `vwc_rock_pct` for rock wool, and `vwc_coco_pct` for coconut peat.
For pore EC, also check the intended use and the applicability conditions in the
product documentation. The Rock wool view hides the `EC pore` row. The Coco view
shows `--` for pore EC when the sensor firmware does not support it or is
unknown; general-soil pore EC is never shown in its place.

The view selection affects only the display, not CSV storage or cloud uploads.
All three VWC conversions and the EC fields are recorded regardless of the
selected view. Coconut pore EC is sent only when the sensor firmware supports
it. The Core2 battery percentage is not sent, but the Core2 battery voltage is
recorded. Uploads go to the HTTPS `/exec` URL in `gas_url` of the active NVS or
SD configuration, and `gas_token` must match the GAS `INGEST_TOKEN`. The Core2
restarts automatically after a successful USB save. After editing the SD
configuration, restart the Core2 manually. Configuration changes need no
rebuild.

The logger waits for sensor conversion by polling without blocking, so the
display and network handling continue during the wait. A single UART
transaction, microSD write, or HTTPS exchange can still pause other processing
briefly.

Successful measurements and sensor communication errors are appended to
`/slt5006.csv`. Error rows have empty measurement values and give the reason in
`error`. Normal rows contain the ISO timestamp, sensor serial number and
firmware, the sensor-reported supply voltage `battery_v`, the Core2 battery
voltage `core2_battery_v`, temperature, the three VWC conversions, the EC fields,
and RSSI. Column names include units.

To copy the CSV, stop the Core2 and remove the card safely first. Do not write a
CSV that was opened in Excel or another application back to the card.

If the firmware does not support coconut pore EC, that field stays empty instead
of showing a false zero. An unavailable Core2 battery voltage is null in JSON
and empty in CSV, never a zero or negative value. RSSI `-127` means that Wi-Fi
is not connected.

The logger checks that every byte of the CSV header and of each row is written.
A partial write is reported as a failure, but a record that was queued
successfully stays available for cloud upload. After an SD write error, a
partial line may remain in the CSV. Stop the Core2, inspect and back up the CSV,
and then resume.

Each pending record is a separate file under `/slt5006_queue`. Before each
batch, the logger scans the entire queue and selects records by UTC `ts`, oldest
first. Records with an empty `ts` (clock not synchronized) are sent before dated
records. Records with the same `ts`, including empty ones, are taken in filename
order. Only one batch (default 10 rows, maximum 20) is held in memory, so memory
use does not grow with the queue size, but scan time does. If no new records are
added, each successful batch deletion drains the backlog oldest-first. This
ordering does not affect the CSV order or format, or the rule that queued
records are deleted only after a confirmed success response.

Each queued JSON file is limited to 2048 bytes, and existing record IDs are not
overwritten. Files that are oversized, cannot be read completely, contain NUL,
or hold malformed JSON (including trailing non-JSON data) stay on the card. The
logger excludes them from upload batches and reports their names on USB Serial.
If the queue directory cannot be opened, or only rejected files remain, the
logger reports a failure instead of an empty-queue success.

Each valid record keeps the same `id` across retries. A file is removed only
after an HTTPS 2xx response **and** a final GAS JSON `ok:true`. After a failure,
a timeout, or an ambiguous response, the file stays queued, and the record is
sent again with the same ID on the next attempt or when you press C. If a write
succeeded but the response was lost, GAS recognizes the ID and does not add a
duplicate row.

GAS result redirects (302/303) are fetched with GET over a new connection. The
`[UPLOAD] hop=... method=... status=...` line on USB Serial shows whether the
original POST or a result GET failed. Redirects to non-HTTPS destinations are
rejected. Each request uses a non-reusable connection and explicitly closes the
TLS connection before HTTP cleanup. This avoids an unnecessary receive probe
when a redirect body is discarded. Closing the connection this way does not hide
errors that occur during POST, GET, or response-body reads.

Before blocking work, the logger redraws the screen and shows
`Connecting Wi-Fi...`, `Preparing upload...`, `Uploading ... row(s)...`, or
`Fetching upload result...`, separately from `Measuring...`. Uploads are
synchronous, so buttons are not handled during HTTPS requests.
`[UPLOAD] accepted_rows=... queue_cleaned=1` confirms both the success response
check and the removal of the acknowledged batch from the SD queue.

Queued sensor failures are uploaded too. A row with serial number 0, empty
firmware and measurement fields, and `sensor response timed out` in `error`
records an earlier attempt in which the sensor information could not be read.
Use its ID and timestamp to tell it apart from current successful measurements.

The queue holds live operating data. Do not delete or edit queue files or change
their IDs manually. To guard against card failure or a full card, back up the
CSV regularly to other media, and monitor the number of queue files and the free
space on the card.

### Upload connection errors

Wi-Fi `online` only means that the Core2 is connected to the access point; it
does not show whether GAS is reachable. Negative upload results (such as -1) are
transport errors from the ESP32 networking library, not HTTP status codes from
the server. To find the cause, open Serial Monitor at 115200 baud and press C to
retry. The `[UPLOAD]` line reports the transport error description, TLS error
code and description, Wi-Fi status, RSSI, and clock validity. It does not
include the URL, token, or payload.

If `clock_valid=0`, check NTP time synchronization. For TLS errors, check
certificate validation, the clock, and the network path. DNS or TCP connections
can fail even when the TLS error code is 0. Keep certificate validation enabled
and use the diagnostics to find the cause. Rows that fail to upload stay in the
SD queue.

The logger embeds GTS Root R1 and R4 as trust anchors for Google. TLS -9984
means that certificate verification failed. Causes include missing trust roots,
an incorrect clock, and certificate replacement on the network path.
`clock_valid=1` only checks that the year is 2024 or later, so also compare the
UTC `ts` in the USB measurement JSON with the actual time.

## Deep sleep

`deep_sleep_sec=0` keeps the logger running continuously. A value of 10 or more
selects a power-saving mode: on each boot, the logger immediately attempts one
measurement, stores the result, attempts an upload, and then sleeps for the set
number of seconds. It also sleeps after a failed sensor attempt. The display,
Wi-Fi, buttons, and measurements do not run during sleep. This mode ignores
`sample_interval_sec`. Because each cycle adds a reboot, sensor stabilization,
and Wi-Fi/NTP connection time, the actual interval from one measurement start to
the next is longer than the sleep duration.

Before running on battery power, test the following on the complete hardware:

- Power behavior of the Core2, Bottom, and sensor during sleep
- Absence of back-powering through the UART
- Clock behavior after an NTP failure, Wi-Fi connection time, and current
  consumption
- Sleep entry only after SD writes complete, and file recovery after a power
  loss
- The choice between deep sleep and `sample_interval_sec`, and the actual
  interval from one measurement start to the next

## Acceptance check

1. With `M5Core2QuickStart`, check that the firmware version, serial number,
   temperature, VWC, and EC are displayed.
2. Boot with no configuration in NVS or on SD. Check offline operation and the
   USB setup guidance.
3. Configure over USB. Check the UTC `ts` on USB Serial or in the CSV, and check
   that one row is written to `/slt5006.csv`.
4. Turn off Wi-Fi temporarily, take a measurement, and confirm that it is kept in
   the CSV and the queue.
5. Restore Wi-Fi, press C, and confirm that exactly one row is added to the sheet
   and that the queue shrinks.
6. With the power off, disconnect the sensor, and check that a timeout is
   reported. Then reconnect the sensor with the power off and check recovery.
7. At the actual installation site, test continuous operation, RSSI, supply
   margin, card capacity, and the response to wet and dry conditions.
8. Confirm that the settings persist after a power cycle and after a normal
   firmware upload. Test configuration without SD, invalid input, cancellation,
   and configuration after a reset during deep sleep.
9. Check that the SD configuration applies while NVS is unconfigured, and that
   NVS takes priority after a USB save.
10. Check that unsupported or unknown Coco pore EC shows `--`, that last-good
    values are marked `(STALE)` after a sensor failure, and that SD failures do
    not discard queued rows.
11. With a five-second interval and a test fixture that simulates a slow sensor,
    check that uploads and reconnects still run between long measurements or
    timeouts.

For cloud deployment, see [Google Apps Script setup](gas-setup.md). Before you
incorporate this sample into a larger system, read the
[integration guide](m5stack-integration.md).
