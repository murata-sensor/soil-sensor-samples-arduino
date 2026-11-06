# Security Policy / セキュリティポリシー

## English

Security fixes are provided for the latest released version only.

Do not report a security vulnerability through a public Issue, Discussion, or
Pull Request. Use GitHub's [private vulnerability reporting form](https://github.com/murata-sensor/soil-sensor-samples-arduino/security/advisories/new).

Include the affected library version, Arduino board/core version, sensor and
firmware version, minimal reproduction steps, and potential impact when known.
Remove credentials, tokens, sensor serial numbers, customer data, and other
confidential information from reports and attachments. Do not test devices or
systems that you do not own or have permission to test.

If private reporting is temporarily unavailable, do not disclose the issue in a
public ticket. General defects and usage questions can be reported through
[GitHub Issues](https://github.com/murata-sensor/soil-sensor-samples-arduino/issues).

The M5Stack Cloud Logger receives Wi-Fi credentials, a deployed GAS URL, and a
shared token over USB Serial and stores them in internal NVS without encryption.
When NVS is unconfigured, it can read a plaintext configuration from microSD.
Restrict physical access to the device and card, never commit PC or SD
configuration files, and rotate credentials after suspected exposure.
Keep TLS certificate validation enabled.

## 日本語

Security修正は、最新のrelease versionのみを対象とします。

Security上の脆弱性を公開Issue、Discussion、Pull Requestへ投稿しないでください。
GitHubの[非公開脆弱性報告フォーム](https://github.com/murata-sensor/soil-sensor-samples-arduino/security/advisories/new)
を使用してください。

可能な範囲で、対象library version、Arduino board／core version、センサ品番と
firmware version、最小限の再現手順、想定される影響を記載してください。報告と
添付ファイルから、認証情報、token、センサのserial number、顧客情報、その他の
機密情報を削除してください。所有していない、または試験許可を得ていないdeviceや
systemに対して検証を行わないでください。

非公開報告フォームが一時的に利用できない場合も、公開ticketへ投稿しないで
ください。一般的な不具合や使用方法の質問は[GitHub Issues](https://github.com/murata-sensor/soil-sensor-samples-arduino/issues)
を使用してください。

M5Stack Cloud LoggerはWi-Fi認証情報、デプロイ済みGAS URL、共有tokenをUSBシリアルで
受信し、内蔵NVSへ暗号化せず保存します。Core2への物理アクセスを制限し、
NVS未設定時にはmicroSDの設定ファイルも読み込みます。SD設定も暗号化していません。
PCの非公開設定JSONやSD設定ファイルをcommitせず、カードも安全に保管し、
漏えいが疑われる場合は認証情報を更新してください。TLS証明書検証は無効にしないで
ください。
