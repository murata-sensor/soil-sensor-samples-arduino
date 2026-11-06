# Contributing / コントリビューション

## English

Bug reports, questions, and feature requests are welcome through
[GitHub Issues](https://github.com/murata-sensor/soil-sensor-samples-arduino/issues).
For vulnerabilities, follow [SECURITY.md](SECURITY.md) and do not open a public
issue.

Before opening an issue:

- Search existing issues to avoid duplicates.
- Include the library version, Arduino or M5Stack board and core version,
  SLT5006 firmware, UART implementation, and the smallest reproducible sketch.
- Include exact compiler output or a minimal serial trace when relevant.
- Remove credentials, tokens, serial numbers, customer data, and other
  confidential information from logs and attachments.
- For hardware problems, describe the supply and signal-voltage conditions
  without posting confidential schematics.
- For Cloud Logger problems, remove Wi-Fi credentials, GAS URLs and tokens,
  sensor serial numbers, and recorded data before attaching logs.

This repository does not currently accept external pull requests. Proposed code
changes may be described in an issue for consideration.

### Example naming

Use `Board + Purpose` while only SLT5006 is supported, for example
`UnoBasicMeasurement` or `M5Core2QuickStart`. When another sensor product is
added, rename all examples consistently to `Product + Board + Purpose`, for
example `SLT5006UnoBasicMeasurement` and `SLT5009M5Core2QuickStart`. The sketch
folder and its primary `.ino` file must have the same name.

## 日本語

不具合報告、ご質問、機能要望は[GitHub Issues](https://github.com/murata-sensor/soil-sensor-samples-arduino/issues)
で受け付けています。Security上の脆弱性は[SECURITY.md](SECURITY.md)に従い、
公開Issueへ投稿しないでください。

Issueを作成する前に、以下をご確認ください。

- 既存Issueを検索し、同じ内容がないことを確認してください。
- library version、Arduino／M5Stack board・core version、SLT5006 firmware、使用UART、
  最小限の再現sketchを記載してください。
- 必要に応じて正確なcompiler出力または最小限のserial traceを添付してください。
- logや添付ファイルから、認証情報、token、serial number、顧客情報、その他の
  機密情報を削除してください。
- hardware問題では、機密回路図を投稿せず、電源条件と信号電圧条件を記載してください。
- Cloud Loggerの問題では、Wi-Fi認証情報、GAS URL／token、sensor serial number、
  記録データをlogから削除してください。

現在、外部からのPull Requestは受け付けていません。コード変更のご提案は、
Issueに内容を記載してください。

### サンプルの命名規則

SLT5006だけをサポートする間は、`UnoBasicMeasurement`、`M5Core2QuickStart`の
ように`Board + Purpose`形式を使用します。別のセンサ品番を追加する時点で、
`SLT5006UnoBasicMeasurement`、`SLT5009M5Core2QuickStart`のような
`Product + Board + Purpose`形式へ全サンプルを統一します。sketch folderと主`.ino`
fileは同じ名前にしてください。
