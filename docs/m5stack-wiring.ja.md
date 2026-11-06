# M5Core2とSLT5006の配線

[English](m5stack-wiring.md) | 日本語

このガイドの対象は、TTL UARTインターフェースの**SLT5006を1台**、
M5Stack Core2本体の赤い**PORT.A**、またはM5GO Bottom2等の青い
**PORT.C**へ接続する構成です。外観が似ていても、RS-232Cインターフェースの
SLT5005をこの配線へ接続しないでください。

## 最初に守ること

> 配線の接続・取り外しは、Core2、センサ、外部電源、USBケーブルをすべて
> OFF／取り外した状態で行ってください。通電中の配線変更は禁止です。

1. センサのラベルが`SLT5006`であることを確認します。
2. 使用するセンサの最新データシートで、電源、UART入出力レベル、Enable端子、
   電源シーケンス、ケーブルを確認します。
3. Core2へスケッチを書き込む間はセンサを外しておきます。
4. 電源OFFで配線し、短絡、極性、TX/RXの交差を再確認してから給電します。
5. 未使用線はまとめず、1本ずつ絶縁します。

村田製作所の公開仕様では、SLT5006の電源範囲は3.0～6.0 V、Read/Write時の
動作電流は最大50 mA、UARTへ接続可能なセンサは1台、最大ケーブル長は3 mです。
ただし、UART端子の論理電圧や絶対最大定格は電源仕様とは別です。必ず最新の
[ラインアップ・電気的仕様](https://www.murata.com/ja-jp/products/sensor/soil/design-development/lineup)
と[データシート](https://www.murata.com/ja-jp/products/sensor/soil/design-development/dl)
を確認してください。

## 使用するCore2ポート

Core2本体だけで接続する場合は赤いPORT.AをUARTへ転用します。両サンプルの
既定値はPORT.Aです。M5GO Bottom2等の青いPORT.Cを使う場合はスケッチで切り替えます。

| Core2 PORT.A | ESP32 GPIO | UARTでの役割 | このサンプル |
|---|---:|---|---|
| 黒線 | GND | GND | 共通GND |
| 赤線 | 5 V | 電源 | SLT5006電源 |
| 黄線 | GPIO32 | RX | Core2 RX |
| 白線 | GPIO33 | TX | Core2 TX |

| Core2 PORT.C | ESP32 GPIO | UARTでの役割 | このサンプル |
|---|---:|---|---|
| 黒線 | GND | GND | 共通GND |
| 赤線 | 5 V | 電源 | SLT5006電源 |
| 黄線 | GPIO14 | TXD2 | Core2 TX |
| 白線 | GPIO13 | RXD2 | Core2 RX |

これはM5Stack公式の
[Core2ピンマップ](https://docs.m5stack.com/en/core/core2)および
[M5GO Bottom2ピンマップ](https://docs.m5stack.com/en/base/m5go_bottom2)
に基づく対応です。使用中のBottom／拡張モジュールに青いPORT.Cが実装され、
同じピン配置であることを現物とその資料で確認してください。

> **色名に注意:** PORT.Cケーブルの「白線」とSLT5006ケーブルの「WHITE」は
> 別の線です。SLT5006のWHITEはEnable、PORT.Cの白線はGPIO13/RXです。
> 色同士をそのまま接続しないでください。

## 信号の接続

| SLT5006ケーブル | センサ側の機能 | 接続先 |
|---|---|---|
| RED | センサ電源 | 使用するGROVEケーブルの赤線（5 V） |
| BLACK | GND | 使用するGROVEケーブルの黒線（GND） |
| WHITE | Enable（Highで動作） | このサンプルではSLT5006 REDへ |
| BLUE | Sensor TX | PORT.A黄線/GPIO32、またはPORT.C白線/GPIO13へ |
| YELLOW | Sensor RX | PORT.A白線/GPIO33、またはPORT.C黄線/GPIO14から |
| GRAY / shield | Shield | 安定通信のため共通GNDへの接続を推奨 |
| GREEN | 内部用 | 接続せず個別に絶縁 |
| ORANGE | 内部用 | 接続せず個別に絶縁 |

```text
SLT5006 BLUE   (Sensor TX) ──> PORT.A yellow/GPIO32 or PORT.C white/GPIO13
SLT5006 YELLOW (Sensor RX) <── PORT.A white/GPIO33 or PORT.C yellow/GPIO14
SLT5006 BLACK  (GND)       ─────────────────────────────────── Core2 GND
SLT5006 RED    (Supply)    <────────────────────────────────── Core2 5 V
SLT5006 WHITE  (Enable)    ─────────────────────────────────── SLT5006 RED
```

UART設定は`9600 bps, 8 data bits, parityなし, 1 stop bit, flow controlなし`
（8N1）です。SLT5006はアドレスを持たない1対1接続です。2台のSensor TXを
同じCore2 RXへ接続しないでください。

## 電源とロジック電圧

SLT5006は内部DC-DCにより3.0～6.0 V電源で動作し、UART信号は3.3 Vです。
Core2のESP32 GPIOも3.3 Vロジックのため、この組合せではUARTレベル変換は
不要です。次を回路ごとに確認してください。

- SLT5006 UART RX/TXのVIH、VIL、VOH、VOL、絶対最大定格
- 使用するGPIO32/GPIO33、またはGPIO13/GPIO14の入出力定格
- Core2またはセンサの片側だけがOFFでも信号線から逆給電しない
  powered-off protection
- 起動時と過渡時を含むセンサ電源容量、および両機器の電源シーケンス

GROVEの赤線（5 V）から給電できますが、Core2／Bottomの出力容量、起動時の
過渡電流、USB接続時を含む電源シーケンスを確認してください。GPIOから
センサへ給電しないでください。

## 安全な書き込み・給電手順

1. Core2からSLT5006と外部電源を外します。
2. `M5Core2QuickStart`または`M5Core2CloudLogger`を書き込みます。
3. USBケーブルを外し、Core2の電源をOFFにします。
4. 上表のとおり、承認済みの電源・レベル変換回路を含めて配線します。
5. BLACK、電源GND、Core2 GNDが共通であること、TX/RXが交差していること、
   GREEN/ORANGEが個別に絶縁されていることを確認します。
6. 製品資料と使用回路で決めた順序で給電します。USBの再接続もCore2への
   給電操作として扱います。

保守、コネクタ抜け、電池切れ、USBの抜き差しも「片側電源断」になり得ます。
量産回路では正常起動時だけでなく、これらの故障・保守条件も評価してください。

## PORT.AとPORT.Cの切替

スケッチ冒頭の`SENSOR_PORT`を、使用するコネクタに合わせて変更します。

```cpp
constexpr SensorPort SENSOR_PORT = SensorPort::PORT_A;  // Core2本体 G32/G33
// constexpr SensorPort SENSOR_PORT = SensorPort::PORT_C;  // Bottom2 G13/G14
```

PORT.Aでは`RX=GPIO32, TX=GPIO33`、PORT.Cでは`RX=GPIO13, TX=GPIO14`が
自動的に選択されます。

- Core2本体のPORT.AをUARTへ転用している間、PORT.AのI2C機器は同時使用できません。
- 選択したポートと実配線が異なると応答タイムアウトになります。

GPIOを変えても、SLT5006側の電源・信号レベル・TX/RX交差・Enable条件は変わりません。

## 応答がないとき

電源を切ってから、次の順に確認します。

1. 品番がSLT5006で、接続が1台だけか。
2. BLACKを含むGNDが共通か。WHITE Enableが正しくHighになっているか。
3. BLUE Sensor TXが選択したRX（PORT.A GPIO32／PORT.C GPIO13）へ、
   YELLOW Sensor RXがTX（PORT.A GPIO33／PORT.C GPIO14）から接続されているか。
4. スケッチと実配線のGPIOが一致しているか（32/33と13/14が混在していないか）。
5. UARTが9600 bps、8N1か。
6. 電源電圧、UARTロジック電圧、レベル変換器の方向が仕様を満たすか。
7. 接触、配線長、シールド、ノイズ、GND電位差に問題がないか。

配線後の確認方法は[ロガー設定](logger-setup.ja.md)、通信内容は
[プロトコル資料](m5stack-protocol.ja.md)を参照してください。
