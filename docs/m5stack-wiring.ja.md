# M5Core2とSLT5006の配線

[English](m5stack-wiring.md) | 日本語

このガイドでは、TTL UARTインターフェースの**SLT5006を1台**、M5Stack Core2本体の赤い**PORT.A**、またはM5GO Bottom2などの青い**PORT.C**に接続する方法を説明します。SLT5005はRS-232Cインターフェースです。外観が似ていても、この配線には接続しないでください。

## 最初に守ること

> 配線の接続や取り外しは、Core2、センサ、外部電源をすべてオフにし、USBケーブルを外した状態で行ってください。通電中は配線を変更しないでください。

1. センサのラベルが`SLT5006`であることを確認します。
2. 最新のデータシートで、電源、UART入出力レベル、Enable端子、電源投入順序、ケーブルの仕様を確認します。
3. Core2へスケッチを書き込む間は、センサを外しておきます。
4. 電源を切った状態で配線し、短絡、極性、TX/RXの交差接続を確認してから給電します。
5. 使わない線はまとめずに、1本ずつ絶縁します。

SLT5006の電源電圧は3.0～6.0 V、Read/Write時の動作電流は最大50 mAです。1つのUARTに接続できるセンサは1台で、最大ケーブル長は3 mです。UART端子の論理電圧と絶対最大定格は、電源の仕様とは別に規定されています。詳しくは最新の[ラインアップ・電気的仕様](https://www.murata.com/ja-jp/products/sensor/soil/design-development/lineup)と[データシート](https://www.murata.com/ja-jp/products/sensor/soil/design-development/dl)を参照してください。

## 使用するCore2ポート

Core2本体だけで使う場合は、赤いPORT.AをUARTに転用します。両サンプルの既定値はPORT.Aです。M5GO Bottom2などの青いPORT.Cを使う場合は、スケッチで切り替えます。

| Core2 PORT.A | ESP32 GPIO | UARTでの役割 | サンプルでの用途 |
|---|---:|---|---|
| 黒線 | GND | GND | 共通GND |
| 赤線 | 5 V | 電源 | SLT5006電源 |
| 黄線 | GPIO32 | RX | Core2 RX |
| 白線 | GPIO33 | TX | Core2 TX |

| Core2 PORT.C | ESP32 GPIO | UARTでの役割 | サンプルでの用途 |
|---|---:|---|---|
| 黒線 | GND | GND | 共通GND |
| 赤線 | 5 V | 電源 | SLT5006電源 |
| 黄線 | GPIO14 | TXD2 | Core2 TX |
| 白線 | GPIO13 | RXD2 | Core2 RX |

この対応は、M5Stack公式の[Core2ピンマップ](https://docs.m5stack.com/en/core/core2)と[M5GO Bottom2ピンマップ](https://docs.m5stack.com/en/base/m5go_bottom2)に基づいています。使用するBottomや拡張モジュールに青いPORT.Cがあり、ピン配置が同じであることを、実物と資料で確認してください。

> **線の色に注意:** PORT.Cケーブルの「白線」とSLT5006ケーブルの「WHITE」は、役割の異なる別の線です。SLT5006のWHITEはEnable、PORT.Cの白線はGPIO13（RX）です。色が同じでも、そのまま接続しないでください。

## 各線の接続先

| SLT5006ケーブル | センサ側の機能 | 接続先 |
|---|---|---|
| RED | センサ電源 | 使用するGROVEケーブルの赤線（5 V） |
| BLACK | GND | 使用するGROVEケーブルの黒線（GND） |
| WHITE | Enable（Highで動作） | サンプルではSLT5006のREDに接続 |
| BLUE | センサTX | PORT.Aの黄線（GPIO32）またはPORT.Cの白線（GPIO13）へ |
| YELLOW | センサRX | PORT.Aの白線（GPIO33）またはPORT.Cの黄線（GPIO14）から |
| GRAY / shield | シールド | 通信を安定させるため、共通GNDへの接続を推奨 |
| GREEN | 内部用 | 接続せず、1本ずつ絶縁 |
| ORANGE | 内部用 | 接続せず、1本ずつ絶縁 |

```text
SLT5006 BLUE   (Sensor TX) ──> PORT.A yellow/GPIO32 or PORT.C white/GPIO13
SLT5006 YELLOW (Sensor RX) <── PORT.A white/GPIO33 or PORT.C yellow/GPIO14
SLT5006 BLACK  (GND)       ─────────────────────────────────── Core2 GND
SLT5006 RED    (Supply)    <────────────────────────────────── Core2 5 V
SLT5006 WHITE  (Enable)    ─────────────────────────────────── SLT5006 RED
```

UARTの設定は、9600 bps、データ長8ビット、パリティなし、ストップビット1、フロー制御なし（8N1）です。SLT5006はアドレスを持たないため、1対1で接続します。2台のセンサのTXを同じCore2 RXに接続しないでください。

## 電源とロジック電圧

SLT5006は内部のDC-DCコンバータにより3.0～6.0 Vの電源で動作し、UART信号は3.3 Vロジックです。Core2のESP32 GPIOも3.3 Vロジックのため、この組み合わせではUARTのレベル変換は不要です。回路ごとに次の項目を確認してください。

- SLT5006のUART RX/TXのVIH、VIL、VOH、VOL、絶対最大定格
- 使用するGPIO（GPIO32/GPIO33またはGPIO13/GPIO14）の入出力定格
- Core2とセンサのどちらか一方だけが電源オフのときに、信号線を通じた逆給電を防ぐ仕組み（powered-off protection）
- 起動時と過渡時を含むセンサ電源の容量と、両機器の電源投入順序

センサにはGROVEケーブルの赤線（5 V）から給電できます。その場合は、Core2やBottomの出力容量、起動時の過渡電流、USB接続時を含む電源投入順序を確認してください。GPIOからセンサへ給電しないでください。

## 安全な書き込み・給電手順

1. Core2からSLT5006と外部電源を外します。
2. `M5Core2QuickStart`または`M5Core2CloudLogger`を書き込みます。
3. USBケーブルを外し、Core2の電源を切ります。
4. 上の表のとおりに、センサの電源を含めて配線します。
5. BLACK、電源のGND、Core2のGNDが共通になっていること、極性が正しいこと、TX/RXが交差していること、GREENとORANGEが1本ずつ絶縁されていることを確認します。
6. 製品資料と回路設計で決めた順序で給電します。USBケーブルの再接続も、Core2への給電として扱います。

保守作業、コネクタの抜け、電池切れ、USBケーブルの抜き差しでも、どちらか一方の電源だけが切れることがあります。量産向けの回路では、通常の起動時だけでなく、こうした故障時や保守時にも逆給電が起きないかを評価してください。

## PORT.AとPORT.Cの切り替え

スケッチ冒頭の`SENSOR_PORT`を、使用するコネクタに合わせて変更します。

```cpp
constexpr SensorPort SENSOR_PORT = SensorPort::PORT_A;  // Core2本体 G32/G33
// constexpr SensorPort SENSOR_PORT = SensorPort::PORT_C;  // Bottom2 G13/G14
```

PORT.Aを選ぶと`RX=GPIO32, TX=GPIO33`、PORT.Cを選ぶと`RX=GPIO13, TX=GPIO14`が自動的に設定されます。

- Core2本体のPORT.AをUARTとして使っている間は、PORT.AのI2C機器を同時に使えません。
- スケッチで選んだポートと実際の配線が異なると、応答がタイムアウトします。

どのGPIOを使っても、SLT5006側の電源、信号レベル、TX/RXの交差接続、Enableの条件は同じです。

## 応答がないとき

電源を切ってから、次の順に確認します。

1. 品番がSLT5006で、接続しているのが1台だけか。
2. BLACKを含めてGNDが共通になっているか。WHITE（Enable）がHighになっているか。
3. BLUE（センサTX）が選択したRX（PORT.AはGPIO32、PORT.CはGPIO13）に、YELLOW（センサRX）が選択したTX（PORT.AはGPIO33、PORT.CはGPIO14）に接続されているか。
4. スケッチのGPIOと実際の配線が一致しているか（32/33と13/14が混在していないか）。
5. UARTの設定が9600 bps、8N1か。
6. 電源電圧とUARTのロジック電圧が仕様を満たしているか。レベル変換器を入れている場合は、変換の向きが正しいか。
7. 接触、配線長、シールド、ノイズ、GND電位差に問題がないか。

配線後の動作確認は[ロガー設定](logger-setup.ja.md)を、通信の詳細は[プロトコル資料](m5stack-protocol.ja.md)を参照してください。
