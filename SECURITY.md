# Security Policy / セキュリティポリシー

## English

We provide security fixes only for the latest released version.

Report security vulnerabilities through GitHub's [private vulnerability reporting form](https://github.com/murata-sensor/soil-sensor-samples-arduino/security/advisories/new).
Do not report them in a public Issue, Discussion, or Pull Request.

Include as much of the following as you know: the affected library version,
the Arduino board and core version, the sensor model and its firmware version,
minimal steps to reproduce, and the potential impact. Remove credentials,
tokens, sensor serial numbers, customer data, and other confidential
information from the report and attachments. Test only devices and systems
that you own or have permission to test.

If private reporting is temporarily unavailable, still do not disclose the
vulnerability in a public ticket. Use
[GitHub Issues](https://github.com/murata-sensor/soil-sensor-samples-arduino/issues)
for general defects and usage questions.

The M5Stack Cloud Logger receives Wi-Fi credentials, a deployed GAS URL, and a
shared token over USB Serial. It stores them in internal NVS without
encryption. If NVS has no configuration, the logger can read a plaintext
configuration file from microSD. Restrict physical access to the device and
the card. Never commit the PC or SD configuration files. If you suspect that
the credentials have been exposed, rotate them. Keep TLS certificate
validation enabled.

## 日本語

セキュリティ修正は、最新のリリースバージョンだけを対象とします。

脆弱性は、GitHubの[非公開脆弱性報告フォーム](https://github.com/murata-sensor/soil-sensor-samples-arduino/security/advisories/new)から報告してください。公開のIssue、Discussion、Pull Requestには投稿しないでください。

わかる範囲で、対象のライブラリバージョン、Arduinoのボードとコアのバージョン、センサの品番とファームウェアバージョン、最小限の再現手順、想定される影響を記載してください。報告と添付ファイルからは、認証情報、トークン、センサのシリアル番号、顧客情報などの機密情報を削除してください。検証は、ご自身が所有する機器・システム、または試験の許可を得た機器・システムだけで行ってください。

非公開報告フォームが一時的に使えない場合も、公開のチケットには投稿しないでください。一般的な不具合や使い方のご質問には、[GitHub Issues](https://github.com/murata-sensor/soil-sensor-samples-arduino/issues)をご利用ください。

M5Stack Cloud Loggerは、Wi-Fiの認証情報、デプロイしたGASのURL、共有トークンをUSBシリアル経由で受け取り、内蔵NVSに暗号化せずに保存します。NVSが未設定の場合は、microSDの設定ファイルも読み込みます。この設定ファイルも暗号化されていません。Core2本体とmicroSDカードへの物理的なアクセスを制限してください。PC上の非公開の設定JSONやSDの設定ファイルは、コミットしないでください。漏えいのおそれがある場合は、認証情報を変更してください。TLS証明書の検証は無効にしないでください。
