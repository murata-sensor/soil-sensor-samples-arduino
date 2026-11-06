# SLT5006 UARTプロトコル

[English](m5stack-protocol.md) | 日本語

この資料は、本リポジトリのドライバがセンサ情報と測定値を取得するために使用する
最小範囲を説明します。完全なレジスタマップ、電気定格、保証条件はSLT5006の
最新製品資料を参照し、顧客公開レジスタ以外へアクセスしないでください。

この通信内容と換算は`soil-sensor-samples-arduino`および
`soil-sensor-samples-python`のSLT5006実装と互換です。

## UART設定

| 項目 | 値 |
|---|---|
| Baud rate | 9600 bps |
| Data bits | 8 |
| Parity | None |
| Stop bits | 1 |
| Flow control | None |
| 接続台数 | 1台 |

SLT5006に機器アドレス／スレーブIDはありません。フレーム先頭は機能コードです。

## フレーム

読出し要求は5 byteです。

```text
01  ADDRESS  SIZE  CRC_H  CRC_L
```

1 byte書込み要求は6 byteです。

```text
02  ADDRESS  01  DATA  CRC_H  CRC_L
```

正常応答は次の形式です。書込み応答は要求したaddress、size、dataをエコーします。

```text
FUNCTION  ADDRESS  SIZE  DATA[SIZE]  CRC_H  CRC_L
```

エラー応答は4 byteです。

```text
(REQUEST_FUNCTION | 0x80)  ERROR_CODE  CRC_H  CRC_L
```

`SIZE`は1～26 byte、最大正常応答は31 byteです。改行、終端文字、STX/ETXは
ありません。受信側は先頭byteのbit 7で4-byteエラー応答を判定し、それ以外は
3-byteヘッダの`SIZE`から残りの長さを決定します。

### 動作確認用の既知フレーム

| 操作 | 要求フレーム（hex） |
|---|---|
| Firmware読出し | `01 00 03 01 60` |
| Serial number読出し | `01 03 04 33 21` |
| 測定開始 | `02 07 01 01 0D 70` |
| 状態読出し | `01 08 01 00 E6` |
| 測定値読出し | `01 11 12 5D AC` |

## CRC

CRC計算はCRC-16/MODBUSと同じです。

- 初期値: `0xFFFF`
- 反転多項式: `0xA001`
- 各data byteをXOR後、LSB-firstで8回シフト

ただし、SLT5006は計算結果を**上位byte、下位byte**の順でワイヤへ送ります。
一般的なMODBUS RTUで使われるlow-byte-firstとは逆です。

擬似コード:

```text
crc = 0xFFFF
for byte in payload:
    crc ^= byte
    repeat 8 times:
        crc = (crc >> 1) ^ 0xA001  if (crc & 1) else (crc >> 1)
send (crc >> 8), (crc & 0xFF)
```

## センサ情報

| Start address | Size | 内容 |
|---|---:|---|
| `0x00` | 3 | Firmware major、minor、revision（各uint8） |
| `0x03` | 4 | Serial number（little-endian uint32） |

Cloud loggerは`firmware`を`major.minor.revision`として保存します。
`serialNumber`はCSVでは10進表記、JSONではnumberとして保存し、付属GASが
スプレッドシート用の文字列へ正規化します。

## 測定シーケンス

1. `0x07`へ`0x01`を1 byte書き、CRC、機能コード、address、size、dataが正しい
   ACKを確認します。
2. ACK後、最初の状態要求まで最低300 ms待ちます。
3. `0x08`を1 byte読み、bit 0が1になるまで200 ms以上の間隔で確認します。
4. 開始ACKから既定30秒に達したら、新しい状態要求を開始せず測定タイムアウト
   とします。個々のUART応答待ちは別に加算される場合があります。
5. 完了後、`0x11`から18 byteを読み、値を換算します。

測定開始ACKは要求が受理されたことだけを示し、測定完了を示しません。

## 測定データ

複数byte値はlittle-endian（low byte first）です。`0x11`から取得する18 byte
（9 words）のうち、ドライバが公開する値は次のとおりです。

| Word | C++メンバー | JSON / CSV | 換算・単位 |
|---:|---|---|---|
| 1 | `batteryVoltageV` | `battery_v` | `raw * 3.3 / 4096 / 0.5` V |
| 2 | `temperatureC` | `temperature_c` | signed low 12 bits × 0.0625 degC |
| 3 | `ecBulkDsM` | `ec_bulk_dsm` | uint16 × 0.001 dS/m |
| 4 | `vwcRockPercent` | `vwc_rock_pct` | uint16 × 0.1 % |
| 5 | `vwcPercent` | `vwc_pct` | uint16 × 0.1 % |
| 6 | `vwcCocoPercent` | `vwc_coco_pct` | uint16 × 0.1 % |
| 8 | `ecPoreDsM` | `ec_pore_dsm` | uint16 × 0.001 dS/m |
| 9 | `ecPoreCocoDsM` | `ec_pore_coco_dsm` | uint16 × 0.001 dS/m |

`batteryVoltageV` / `battery_v`はSLT5006が報告する**センサ側電源電圧の換算値**です。
Core2内蔵電池の電圧はlogger固有の`core2_battery_v`であり、別の値です。

温度は16-bit全体の符号付き整数ではありません。low 12 bitsを2の補数として
符号拡張してから0.0625を掛けます。表にないwordは公開しません。

### Firmwareによる機能差

ココピート向けPore ECはSLT5006 firmware 1.7.5以降で使用できます。

- 1.6.3: `ecPoreCocoDsM`非対応
- 1.7.5、1.7.6: 対応

古いfirmwareでも18 byte全体とword 9を読み出せますが、word 9の値は0であり、
対応測定値ではありません。非対応またはfirmware不明時は画面へ表示せず、固定
schemaのCSV列は空欄、JSON propertyは省略します。

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

エラー応答もCRCと`REQUEST_FUNCTION | 0x80`を検証してから採用します。未知の
コードは捨てず、16進数を含むエラーとして記録してください。

## 実装で必ず検証する項目

- 要求全byteを書き込めたこと
- 応答がタイムアウト内に必要byte数だけ到着したこと（分割到着にも対応）
- 正常応答の`SIZE`が1～26で固定バッファ内に収まること
- CRCと、そのhigh-byte-firstのワイヤ順
- 正常応答の機能コード、address、size
- 書込みACKのaddress、size、dataエコー
- エラー応答の機能コードとCRC
- signed 12-bit温度、little-endian値、換算係数
- 最初の状態要求まで300 ms、以後のpoll間隔、測定全体のタイムアウト
- `millis()`が最大値から0へ戻る場合の経過時間計算

通信エラー後は同じ要求を連打しません。残留受信データを破棄してフレームを
再同期し、間隔を置いて次の測定開始から再試行します。組み込み方針は
[システム組み込みガイド](m5stack-integration.ja.md)を参照してください。
