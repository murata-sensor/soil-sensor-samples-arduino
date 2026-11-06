# Store measurements in Google Sheets

English | [日本語](gas-setup.ja.md)

`M5Core2CloudLogger` can keep each measurement on microSD and upload queued rows
over HTTPS to Google Apps Script (GAS). The included receiver expands named JSON
fields into Google Sheets, locks concurrent writes, and ignores a row ID that
was already stored after a retry.
After receipt, complete data rows (excluding the header) are sorted by `Date`
ascending, including backlog and error rows. `setupSheet` also sorts existing
rows. For unsynchronized records with empty `ts`, Date is the receipt time;
their original measurement order cannot be reconstructed.

The web app normally allows anonymous network access and authenticates writes
with a shared token in the JSON body. Anyone who obtains both URL and token can
write data. Never commit or publish the deployment URL, token, Wi-Fi password,
private configuration JSON, screenshots containing them, or diagnostic logs that
contain them.

## Deploy the receiver

1. Create and open the destination Google spreadsheet.
2. Select **Extensions > Apps Script**.
3. Replace the default `Code.gs` contents with
   [`extras/google-apps-script/Code.gs`](../extras/google-apps-script/Code.gs),
   then save it. The repository's `appsscript.json` is only needed for source
   management tools such as `clasp`.
4. Open **Project Settings > Script properties** and add:

   Generate a hard-to-guess token with Windows PowerShell (32 bytes / 64
   hexadecimal characters):

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

   Save the generated value in a password manager or another secure backup, then
   use the exact same value for GAS `INGEST_TOKEN` and the Core2 `gas_token`.

   | Property | Required | Value |
   |---|---|---|
   | `INGEST_TOKEN` | Yes | A private random token, preferably at least 32 bytes |
   | `TARGET_SHEET` | No | Destination tab; default `soil_sensor` |
   | `TIME_ZONE` | No | IANA display zone; default `Asia/Tokyo` |
   | `SPREADSHEET_ID` | No | Only for a standalone rather than bound script |

5. Select `setupSheet` in the function list and run it once. Approve spreadsheet
   access, then verify that the destination tab and header appeared.
6. Select **Deploy > New deployment > Web app**. Execute as yourself and choose
   the access setting that permits the anonymous Core2 to call it (normally
   **Anyone**). A Workspace administrator may restrict this option.
7. Copy the final HTTPS URL ending in `/exec`.

When changing the GAS code, edit the existing deployment and select **New
version**. Creating an unrelated deployment changes the URL.

Opening `/exec` in a browser returns a non-secret health response. It proves
reachability, not sheet-write authorization:

```json
{"ok":true,"service":"slt5006-google-sheets-receiver","schemaVersion":2}
```

## Configure the Core2

Connect USB, close Serial Monitor, and provision internal NVS from the PC:

```powershell
py -m pip install pyserial
py .\scripts\configure_logger.py --port COM4
```

Set `gas_token` to the exact `INGEST_TOKEN` value and use only an HTTPS `/exec`
URL in the prompts. Reset the Core2 while the tool is waiting if asleep or busy.
Successful saving restarts it automatically. microSD stores logs and the
retry queue and can supply `/slt5006.cfg` when NVS is unconfigured.
See [logger setup](logger-setup.md) for JSON/SD input, defaults, limits
and NVS persistence. NVS is not encrypted by this sample; restrict device access.

Start the wired device, take a measurement, and use touch button C to request an
upload. A successful final response has `ok:true`; HTTP 2xx by itself is not
enough. Pending files remain queued after any ambiguous or failed response.

## Request contract

The serial number and record ID below are fictional examples.

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

`token` and `serialNumber` are required. Send 1-100 rows. Normally `ts` is an
ISO 8601 value with `Z` or an offset. A provided `rows` must be an array;
the compatible single-row form is accepted only when `rows` is omitted.
Identifiers must be strings or finite numbers, not booleans or objects.
If `ts` is empty, GAS uses server receipt
time and `id` becomes required; the logger marks that row
`time_not_synchronized`. If `id` is otherwise absent, the receiver derives one
from serial number and normalized timestamp. Retried records retain the ID.
Calendar dates are validated, including leap years. Fractional seconds from
Python and .NET are accepted; derived IDs use UTC millisecond precision.
Provide a unique explicit ID when measurements could share that precision.

All measurement fields are optional finite numbers or numeric strings.
Omitted, null, and empty values leave cells empty; whitespace-only numeric
strings are rejected rather than converted to zero. `battery_v` is the sensor-
reported supply; `core2_battery_v` is the Core2 battery. Omit unavailable values.
In particular, omit `ec_pore_coco_dsm` for unsupported or unidentified sensor
firmware rather than reporting a false zero. Sheet columns are independent of
JSON key order:

```text
Date, SerialNumber, Firmware, Battery(V), Temperature(degC), VWC(%), VWC rock(%), VWC coco(%), EC bulk(dS/m), EC pore(dS/m), EC pore coco(dS/m), Id, Core2 Battery(V), WiFi RSSI(dBm), Error
```

To recreate a disposable test sheet with this layout, stop Core2, delete only
the receiver tab (default `soil_sensor`), and run `setupSheet` with the updated
code. Do not delete the whole spreadsheet or its bound script project. If this
is the last tab, add a temporary empty tab first. Existing data will be lost.
Update the existing web-app deployment to a new version before restarting Core2.
Past rows still queued on SD will be resent.

The receiver validates the entire request before writing, acquires a script
lock, filters existing IDs, and writes the remaining rows in one operation.

```json
{"ok":true,"received":10,"written":8,"duplicates":2}
```

Errors have `ok:false`. Apps Script may still transport such a JSON error with
HTTP 200, so clients must inspect the final body after redirects.

## Optional receiver-only test (PowerShell)

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

Expect `written=1`. Sending the identical `$body` again should return
`written=0` and `duplicates=1`. Delete the `TEST001` test row afterward.

## Operational safety

- Restrict physical access to the device and config card; neither NVS nor SD
  credentials are encrypted by this sample. Rotate both Script Property and
  all device tokens after exposure.
- Keep TLS certificate validation enabled. A port must safely follow the GAS
  response redirect and parse the final JSON.
- Limit spreadsheet sharing, enable multi-factor authentication, and review GAS
  execution history.
- Shared-token ingestion is suitable for evaluation and small deployments. Use
  an API with per-device credentials, revocation, and auditing when those are
  requirements.

For errors, verify exact token equality, `/exec` URL, time synchronization,
`setupSheet`, the target header, and that code changes were deployed as a new
version. Do not paste tokens or Wi-Fi passwords into support logs.
