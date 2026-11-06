# SLT5006 UARTプロトコル

[English](protocol.en.md) | 日本語

この資料は、本ライブラリが動作確認と測定に使用する最小範囲を説明します。完全なレジスタマップ、電気定格、保証条件はSLT5006の最新製品資料を参照し、顧客公開レジスタ以外へアクセスしないでください。

## UART設定

| 項目 | 値 |
|---|---|
| Baud rate | 9600 bps |
| Data bits | 8 |
| Parity | None |
| Stop bits | 1 |
| Flow control | None |
| 接続台数 | 1台 |

機器アドレス／スレーブIDはありません。先頭バイトは機能コードです。

## フレーム

読出し要求は5 byteです。

```text
01  ADDRESS  SIZE  CRC_H  CRC_L
```

1 byte書込み要求は6 byteです。

```text
02  ADDRESS  01  DATA  CRC_H  CRC_L
```

正常応答は読出し、書込みとも同じ形式で、書込み応答は要求内容をエコーします。

```text
FUNCTION  ADDRESS  SIZE  DATA[SIZE]  CRC_H  CRC_L
```

エラー応答は4 byteです。

```text
(REQUEST_FUNCTION | 0x80)  ERROR_CODE  CRC_H  CRC_L
```

`SIZE`は1～26 byteです。最大正常応答は31 byteになります。改行、終端文字、STX/ETXはありません。受信側は先頭1 byteのbit 7でエラー応答を判定し、それ以外では3 byteのヘッダーから残りの長さを決定します。

## CRC

計算パラメータはCRC-16/MODBUSです。

- 初期値: `0xFFFF`
- 反転多項式: `0xA001`
- 各データbyteをXORした後、LSB-firstで8回シフト

ただし、SLT5006のワイヤ上では算出した16-bit CRCを **上位byte、下位byte** の順に送ります。通常のMODBUS RTUで一般的なlow-byte-firstと逆なので注意してください。

既知の要求フレーム:

| 操作 | Hex frame |
|---|---|
| Firmware読出し | `01 00 03 01 60` |
| Serial読出し | `01 03 04 33 21` |
| 測定開始 | `02 07 01 01 0D 70` |
| 状態読出し | `01 08 01 00 E6` |
| 測定値読出し | `01 11 12 5D AC` |

## センサ情報

| Start address | Size | 内容 |
|---|---:|---|
| `0x00` | 3 | Firmware major, minor, revision（各uint8） |
| `0x03` | 4 | Serial number（little-endian uint32） |

## 測定手順

1. `0x07`へ`0x01`を1 byte書き込み、正常ACKを確認します。
2. ACK後、最初の状態読出しまで最低300 ms待ちます。
3. `0x08`を1 byte読み、bit 0が1になるまで200 ms間隔で確認します。
4. 開始ACK後の経過時間が既定30秒に達したら、新しい状態読出しを開始せず測定タイムアウトとします。個々のUART応答待ちは別に加算されます。
5. 完了後、`0x11`から18 byteを読み、換算済み測定値を返します。

測定開始の書込みACKは「要求を受理した」ことを示し、測定完了を示すものではありません。

## データ形式

複数byte値はすべてlow byte firstです。

`0x11`からの18 byte（9 words）のうち、本ライブラリが公開する値:

| Word | 内容 | 換算 |
|---:|---|---|
| 1 | Supply voltage | 内部ADC値を`raw * 3.3 / 4096 / 0.5` Vで換算し、Vだけを公開 |
| 2 | Temperature | low 12 bitsを2の補数で符号化し、`* 0.0625` degC |
| 3 | Bulk EC | `uint16 * 0.001` dS/m |
| 4 | VWC rock wool | `uint16 * 0.1` % |
| 5 | VWC | `uint16 * 0.1` % |
| 6 | VWC coco peat | `uint16 * 0.1` % |
| 8 | Pore-water EC | `uint16 * 0.001` dS/m |
| 9 | Pore EC coco peat | `uint16 * 0.001` dS/m |

Pore EC coco peatはSLT5006 firmware 1.7.5以降で使用できます。古いfirmwareでも18 byte全体とword 9を読み出せますが、word 9の値は0であり、対応測定値として顧客データに使用しません。

表にないwordは本ライブラリでは使用・公開しません。

## センサエラー

| Code | 意味 |
|---:|---|
| `0x01` | Illegal function code |
| `0x02` | Illegal start address |
| `0x03` | Illegal byte size |
| `0x04` | Internal receive-buffer overflow |
| `0x05` | CRC-16 error |
| `0x06` | Measurement中のdata read |
| `0x10` | Internal storage write failure |
| `0x20` | Internal sensor communication error |
| `0x40` | Sensor-side measurement timeout |

エラー応答でもCRCと`REQUEST_FUNCTION | 0x80`を検証してからコードを採用します。未知のコードは16進数のまま記録してください。

## 受信側で検証する項目

- 要求全byteを書き込めたこと
- 必要な応答byte数をタイムアウト内に受信したこと
- 応答の`SIZE`が26以下で受信バッファ内に収まること
- CRC
- 正常応答の機能コード、address、size
- 書込みACKのaddress、size、dataエコー
- エラー応答の機能コード

本ライブラリはこれらをすべて検証します。
