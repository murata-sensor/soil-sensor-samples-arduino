# Contributing / コントリビューション

## English

Please submit bug reports, questions, and feature requests through
[GitHub Issues](https://github.com/murata-sensor/soil-sensor-samples-arduino/issues).
To report a security vulnerability, follow [SECURITY.md](SECURITY.md) instead
of opening a public issue.

Before opening an issue:

- Search existing issues to avoid duplicates.
- Include the library version, the Arduino or M5Stack board and its core
  version, the SLT5006 firmware version, the UART implementation, and the
  smallest sketch that reproduces the problem.
- For build errors, include the exact compiler output. For problems at run
  time, include a minimal serial trace.
- Remove credentials, tokens, serial numbers, customer data, and other
  confidential information from logs and attachments.
- For hardware problems, describe the power supply and signal-voltage
  conditions. Do not post confidential schematics.
- For Cloud Logger problems, also remove Wi-Fi credentials, GAS URLs and
  tokens, sensor serial numbers, and recorded data from logs before you attach
  them.

We do not currently accept external pull requests. To propose a code change,
describe it in an issue.

### Example naming

While SLT5006 is the only supported sensor, name examples `Board + Purpose`,
for example `UnoBasicMeasurement` or `M5Core2QuickStart`. When another sensor
product is added, rename all examples to `Product + Board + Purpose`, for
example `SLT5006UnoBasicMeasurement` and `SLT5009M5Core2QuickStart`. The sketch
folder and its main `.ino` file must have the same name.

## 日本語

不具合報告、ご質問、機能要望は[GitHub Issues](https://github.com/murata-sensor/soil-sensor-samples-arduino/issues)で受け付けています。セキュリティ上の脆弱性は公開Issueに投稿せず、[SECURITY.md](SECURITY.md)の手順で報告してください。

Issueを作成する前に、次の点をご確認ください。

- 既存のIssueを検索し、同じ内容がないことを確認してください。
- ライブラリのバージョン、Arduino／M5Stackのボードとコアのバージョン、SLT5006のファームウェアバージョン、使用しているUART、問題を再現できる最小限のスケッチを記載してください。
- ビルドエラーの場合はコンパイラの出力をそのまま、実行時の問題の場合は最小限のシリアルログを添付してください。
- ログや添付ファイルからは、認証情報、トークン、シリアル番号、顧客情報などの機密情報を削除してください。
- ハードウェアの問題では、電源の条件と信号電圧の条件を記載してください。機密の回路図は投稿しないでください。
- Cloud Loggerの問題では、Wi-Fiの認証情報、GASのURLとトークン、センサのシリアル番号、記録データもログから削除してください。

現在、外部からのPull Requestは受け付けていません。コードの変更を提案する場合は、Issueに内容を記載してください。

### サンプルの命名規則

対応センサがSLT5006だけの間は、`UnoBasicMeasurement`や`M5Core2QuickStart`のように、`Board + Purpose`の形式で命名します。別のセンサ品番を追加するときは、全サンプルの名前を`SLT5006UnoBasicMeasurement`や`SLT5009M5Core2QuickStart`のような`Product + Board + Purpose`の形式に統一します。スケッチのフォルダーと、そのメインの`.ino`ファイルは同じ名前にしてください。
