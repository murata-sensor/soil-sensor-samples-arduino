# Store measurements in Google Sheets

English | [日本語](gas-setup.ja.md)

`M5Core2CloudLogger` saves each measurement to microSD and can upload the queued
rows over HTTPS to Google Apps Script (GAS). The included receiver writes each
measurement as one row in Google Sheets, with the named JSON fields mapped to
columns. It serializes concurrent requests with a script lock and skips any row
whose ID is already stored, so a retry does not add duplicate rows.

After receiving data, the receiver sorts all rows below the header by `Date` in
ascending order, including resent rows from the queue and rows with errors.
Running `setupSheet` also sorts existing rows. If a measurement was taken while
the clock was not synchronized, its `ts` is empty and `Date` holds the receipt
time, so the original measurement order of such rows cannot be restored.

The web app normally accepts anonymous requests and authenticates writes with a
shared token in the JSON body. The URL alone does not allow writes, but anyone
who has both the URL and the token can write data. Never commit or publish the
deployment URL, token, Wi-Fi password, or private configuration JSON. This
includes screenshots, diagnostic logs, and chat messages that contain them.

## Deploy the receiver

1. Create and open the destination Google spreadsheet.
2. Select **Extensions > Apps Script**.
3. Replace the contents of the default `Code.gs` with
   [`extras/google-apps-script/Code.gs`](../extras/google-apps-script/Code.gs)
   and save. You need the repository's `appsscript.json` only if you manage the
   source with a tool such as `clasp`.
4. Generate a hard-to-guess token. In Windows PowerShell, these commands
   generate a 32-byte token (64 hexadecimal characters):

   ```powershell
   $bytes = New-Object byte[] 32
   $rng = [System.Security.Cryptography.RandomNumberGenerator]::Create()
   try {
     $rng.GetBytes($bytes)
     $token = [BitConverter]::ToString($bytes).Replace('-', '').ToLowerInvariant()
     $token
   }
   finally {
     $rng.Dispose()
   }
   ```

   Store the value in a password manager or another secure place. Use exactly
   the same value for the GAS `INGEST_TOKEN` and the Core2 `gas_token`.

   Open **Project Settings > Script properties** and add these properties:

   | Property | Required | Value |
   |---|---|---|
   | `INGEST_TOKEN` | Yes | Private random token, preferably 32 bytes or longer |
   | `TARGET_SHEET` | No | Destination sheet (tab) name. Default: `soil_sensor` |
   | `TIME_ZONE` | No | IANA time zone for displayed dates. Default: `Asia/Tokyo` |
   | `SPREADSHEET_ID` | No | Spreadsheet ID. Set it only for a standalone script, not for a bound script |

5. Select `setupSheet` in the function list and run it once. Approve
   spreadsheet access when prompted, then check that the destination tab and
   its header row were created.
6. Select **Deploy > New deployment > Web app**. Execute as yourself, and
   choose the access setting that lets the Core2 call the web app anonymously
   (normally **Anyone**). If you cannot select it with a Google Workspace
   account, check your administrator's policy.
7. Copy the final HTTPS URL ending in `/exec`.

After you change the GAS code, select **Deploy > Manage deployments**, edit the
existing deployment, and select **New version**. A separate new deployment gets
a different URL, and you then have to change the Core2 setting as well.

Opening the `/exec` URL in a browser returns the following health response. It
needs no token and contains no secrets:

```json
{"ok":true,"service":"slt5006-google-sheets-receiver","schemaVersion":2}
```

This response only shows that the web app is reachable. It does not test the
token or writing to the sheet.

## Configure the Core2

Connect the Core2 to the PC by USB, close Serial Monitor, and run the following
commands on the PC. The tool saves the Wi-Fi and cloud settings to the Core2's
internal NVS:

```powershell
py -m pip install pyserial
py .\scripts\configure_logger.py --port COM4
```

At the prompts, set `gas_token` to the same value as `INGEST_TOKEN`, and always
use the HTTPS URL ending in `/exec` for `gas_url`. If the Core2 is asleep or
busy while the tool is waiting, reset the Core2. After the settings are saved,
the Core2 restarts automatically. When NVS is not configured, the logger can
read its settings from `/slt5006.cfg` on microSD. The microSD card also stores
the logs and the retry queue. See [logger setup](logger-setup.md) for USB
provisioning details, JSON and SD input, defaults and limits, NVS persistence,
and offline behavior.

With the sensor wired, start the Core2, take a measurement, and touch button C
to request an upload. The logger counts an upload as successful only when the
final response contains `ok:true`. HTTP 2xx alone is not enough. If an upload
fails or its result is unclear, the pending files stay in the queue.

## Request format

Normally, send rows in the multi-row format below. The serial number and record
ID are fictional examples.

```json
{
  "token": "shared-secret",
  "rows": [{
    "id": "example-measurement-001",
    "ts": "2026-08-12T10:15:30+09:00",
    "serialNumber": "12345678",
    "firmware": "1.7.6",
    "battery_v": 4.97,
    "core2_battery_v": 4.08,
    "temperature_c": 24.5,
    "vwc_pct": 31.2,
    "vwc_coco_pct": 30.8,
    "vwc_rock_pct": 32.1,
    "ec_bulk_dsm": 0.431,
    "ec_pore_dsm": 1.105,
    "ec_pore_coco_dsm": 1.087,
    "rssi_dbm": -61,
    "error": ""
  }]
}
```

`token` and `serialNumber` are required. A request can contain 1 to 100 rows.
If you include `rows`, it must be an array. The single-row form, kept for
compatibility, is accepted only when `rows` is omitted. `serialNumber` and `id`
must be strings or finite numbers, not booleans or objects.

`ts` is normally an ISO 8601 timestamp with `Z` or a UTC offset. The receiver
rejects dates that do not exist, taking leap years into account, and accepts
fractional seconds from Python and .NET. If `ts` is empty, GAS uses the server
receipt time and `id` is required. The logger marks such a row
`time_not_synchronized`.

If `id` is omitted, the receiver derives it from the serial number and the
normalized timestamp at UTC millisecond precision. If two measurements from the
same sensor can have the same timestamp at that precision, give each a unique
`id`. The logger keeps a row's ID when it retries the upload.

All measurement fields are optional. Each takes a finite number or a numeric
string. Omitted, null, and empty values leave the cell empty. A string that
contains only whitespace is rejected, not converted to zero. `battery_v` is the
supply voltage reported by the sensor, and `core2_battery_v` is the Core2
battery voltage. Omit any value that is not available. In particular, if the
sensor firmware does not support `ec_pore_coco_dsm` or cannot be identified,
omit the field instead of sending a false zero.

The sheet column order does not depend on the JSON key order:

```text
Date, SerialNumber, Firmware, Battery(V), Temperature(degC), VWC(%), VWC rock(%), VWC coco(%), EC bulk(dS/m), EC pore(dS/m), EC pore coco(dS/m), Id, Core2 Battery(V), WiFi RSSI(dBm), Error
```

To recreate a test sheet after the column layout changes, stop the Core2,
delete only the receiver tab (default `soil_sensor`), and run `setupSheet` with
the updated code. Do not delete the whole spreadsheet or its bound Apps Script
project. If the receiver tab is the only tab, add a temporary empty tab first
so that you can delete it. The data in the deleted tab is lost. Update the
existing web-app deployment to a new version before you restart the Core2. Rows
still queued on the SD card are then resent.

The receiver validates every row in a request before it writes anything. It
then acquires a script lock, skips IDs that are already in the sheet, and writes
the remaining rows in one operation. If any row is invalid, the receiver writes
none of the rows in that request. A successful response looks like this:

```json
{"ok":true,"received":10,"written":8,"duplicates":2}
```

An error response has `ok:false`. Apps Script can return such an error with
HTTP 200, so a client must check the JSON body of the final response after
redirects.

## Test only the receiver from PowerShell (optional)

This script prompts for the token, so the token is not saved in the command
history.

```powershell
$url = 'https://script.google.com/macros/s/REPLACE_WITH_DEPLOYMENT_ID/exec'
$secureToken = Read-Host 'INGEST_TOKEN' -AsSecureString
$token = [System.Net.NetworkCredential]::new('', $secureToken).Password
$rowId = 'manual-test-' + [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()
$body = @{
  token = $token
  rows = @(@{
    id = $rowId
    ts = [DateTimeOffset]::Now.ToString('o')
    serialNumber = 'TEST001'
    firmware = 'test'
    temperature_c = 20.0
    vwc_pct = 30.0
    ec_bulk_dsm = 0.1
  })
} | ConvertTo-Json -Depth 4
Invoke-RestMethod -Method Post -Uri $url -ContentType 'application/json' -Body $body
```

A successful request returns `ok=true` and `written=1`. If you send the same
`$body` again and get `written=0` and `duplicates=1`, duplicate detection works.
Afterward, delete the `TEST001` test row from the spreadsheet manually.

## Operational safety

- This sample encrypts neither NVS nor the credentials on the SD card. Restrict
  physical access to the device and the configuration card. If a token is
  exposed, replace both the `INGEST_TOKEN` script property and the `gas_token`
  on every device.
- Keep TLS certificate validation enabled. If you port the client to another
  platform, follow the GAS redirect safely, parse the final JSON, and check
  `ok`.
- Limit spreadsheet sharing, especially edit access, and enable multi-factor
  authentication on the Google account. Review these settings and the Apps
  Script execution history regularly.
- The shared-token method is intended for evaluation and small deployments. If
  you need per-user or per-device credentials, revocation, or auditing, or if
  the data is confidential, move to an API platform with authentication.

If an upload fails, check that the tokens match exactly, that the URL ends in
`/exec`, that the clock is synchronized, that `setupSheet` has been run, that
the target sheet has the expected header, and that code changes were deployed
as a new version. Do not paste tokens or Wi-Fi passwords into logs that you
share for support.
