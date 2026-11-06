# Google Apps Script receiver

`Code.gs` is the optional HTTPS receiver used by `M5Core2CloudLogger`. It accepts
the same named measurement fields as the Python sample uploader, appends them
to Google Sheets, serializes concurrent requests with a script lock, and skips
already-stored row IDs when a device retries a batch.

Do not put a Wi-Fi password, deployment URL, or ingest token in this directory.
Store the ingest token as the `INGEST_TOKEN` Apps Script property and on the
device's internal NVS using USB provisioning. When NVS is unconfigured, the logger
can read a private `/slt5006.cfg` from microSD, which also stores logs and queued
rows. Neither configuration store is encrypted by this sample.

Setup instructions:

- [Japanese](../../docs/gas-setup.ja.md)
- [English](../../docs/gas-setup.md)
