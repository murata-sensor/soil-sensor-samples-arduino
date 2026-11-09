# Google Apps Script receiver

`Code.gs` is the optional HTTPS receiver for `M5Core2CloudLogger`. It accepts
the same named measurement fields as the Python sample uploader and appends them
to Google Sheets. It serializes concurrent requests with a script lock and skips
row IDs that are already stored when a device retries a batch.

Do not put a Wi-Fi password, deployment URL, or ingest token in this directory.
Store the ingest token in the `INGEST_TOKEN` Apps Script property and, through
USB provisioning, in the device's internal NVS. When NVS is not configured, the
logger can read a private `/slt5006.cfg` from microSD. The microSD card also
stores logs and queued rows. This sample encrypts neither NVS nor
`/slt5006.cfg`.

Setup instructions:

- [Japanese](../../docs/gas-setup.ja.md)
- [English](../../docs/gas-setup.md)
