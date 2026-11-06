# SLT5006配線ガイド

[English](wiring.en.md) | 日本語

## 安全に接続する

1. SLT5006の品番を確認します。SLT5005は同じ通信内容でもRS-232C電気レベルのため、このUART配線を使用できません。
2. センサ、Arduino、外部電源をすべてOFFにします。
3. 公式の[ラインアップ・電気的仕様](https://www.murata.com/ja-jp/products/sensor/soil/design-development/lineup)と[データシートダウンロード](https://www.murata.com/ja-jp/products/sensor/soil/design-development/dl)から、電源範囲、必要電流、UARTロジック電圧、ケーブル仕様、電源シーケンスを確認します。
4. 電源線とGNDを接続し、次に信号線を接続します。
5. 未使用線を1本ずつ絶縁します。
6. 短絡と極性を再確認し、製品資料と使用回路で規定された順序で給電します。

通電中に線を接続・取り外ししないでください。

## ケーブル

| 色 | SLT5006での機能 | 接続 |
|---|---|---|
| RED | Supply | 定格を満たす3.0～6.0 V電源 |
| BLACK | GND | センサ電源GNDとArduino GND |
| WHITE | Enable（Highで動作、Lowでstandby） | このサンプルではREDへ |
| BLUE | Sensor TX | Arduino RXへ（必要ならレベル変換経由） |
| YELLOW | Sensor RX | Arduino TXから（必要ならレベル変換経由） |
| GREEN | 内部用 | 接続せず個別に絶縁 |
| ORANGE | 内部用 | 接続せず個別に絶縁 |
| GRAY / shield | Shield | 安定通信のためGND接続を推奨 |

Unoの`UnoBasicMeasurement`はD10をRX、D11をTXとして使用します。

```text
SLT5006 BLUE  (TX) ── level conversion if required ──> Uno D10 (RX)
SLT5006 YELLOW(RX) <── level conversion if required ── Uno D11 (TX)
SLT5006 BLACK (GND) ────────────────────────────────── Uno GND
sensor supply GND   ────────────────────────────────── Uno GND
```

## 電圧について

センサの電源電圧とUART信号電圧は別の仕様です。電源が3.0～6.0 Vの範囲内であっても、UART入力がArduino Unoの5 V信号を許容するとは限りません。公開仕様上の動作電流は最大50 mA（Read/Write cycle）ですが、電源は起動・過渡条件と余裕も含めて選定します。次を個別に確認してください。

- SLT5006の電源電圧と電源容量
- SLT5006 UART RX/TXの論理High/Low条件と絶対最大定格
- Arduino側UARTの信号電圧
- 使用するレベル変換回路が9600 bpsのpush-pull UARTに対応し、TX/RXそれぞれの信号方向と電圧を満たすこと
- 一方の電源がOFFでも信号から逆給電しないpowered-off protection、または両電源が安定するまで出力をhigh-impedanceに保つ仕組み

ArduinoのI/Oピンからセンサへ給電しないでください。Arduino基板上の電源端子を使う場合も、その基板、USB電源、レギュレータ、配線の容量を確認してください。

WHITEをGPIO制御する設計では、Enable端子のVIH/VIL、絶対最大定格、GPIO電圧、片側電源OFF時の注入電流を確認し、必要なドライバ／レベル変換を入れます。Unoの5 V GPIOへ直接接続できるとは仮定しません。本サンプルはstandby制御を行わないため、製品資料で認められた方法としてWHITEをREDへ接続します。

## 書込み・配線・給電の順序

`SoftwareSerial`はArduinoの起動時からTXピンをHigh出力にする場合があります。未給電のSLT5006を信号線へ接続したまま、ArduinoだけをUSB給電してスケッチを書き込まないでください。

1. SLT5006を接続していないArduinoへ先にスケッチを書き込みます。
2. USBを外し、Arduino、センサ、外部電源をすべてOFFにします。
3. 電源・レベル変換回路を含めて配線します。
4. powered-off protectionを持つ検証済み回路、または両側の電源が安定するまでUARTをhigh-impedanceに保つ回路を使用します。
5. 製品資料と回路設計で規定した順序で給電します。

このリポジトリは特定の電源、レベル変換器、電源シーケンスを認定しません。評価回路はSLT5006とArduino双方の定格を満たすことを回路設計者が確認してください。

## 実運用時

- UnoのD0/D1はUSBシリアルモニタと共用されるため、クイックスタートではD10/D11の`SoftwareSerial`を使います。
- `SoftwareSerial`は同時に1ポートだけが受信可能で、他の割込み依存機能と干渉する場合があります。
- ハードウェアUARTを持つボードでは`Serial1`等を優先してください。ピン番号はボード資料で確認します。
- SLT5006は1台接続専用です。2台以上のTX出力を同じRX線へ接続しないでください。
- 公式ラインアップ上、SLT5006 UARTへ接続できる最大ケーブル長は3 mです。この範囲内でもノイズ環境、引き回し、シールド、GND電位差を評価してください。
