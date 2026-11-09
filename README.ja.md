# soil-sensor-samples-arduino

[English (default)](README.md) | 日本語

村田製作所の土壌センサ **SLT5006** を、Arduino Uno／NanoとM5Stack Core2で使うためのサンプルと、再利用できるUARTドライバです。

| 対象ボード | 最初に使うサンプル | 応用例（任意） |
|---|---|---|
| Arduino Uno / Classic Nano | `UnoBasicMeasurement` | `UnoCooperativePolling` |
| M5Stack Core2 | `M5Core2QuickStart` | `M5Core2CloudLogger` |

まず、使用するボードの「最初に使うサンプル」で動作を確認してください。応用例はそれぞれ目的が異なります。`UnoCooperativePolling`は別の測定制御APIの使い方を示す例、`M5Core2CloudLogger`は測定値の記録とクラウドへのアップロードを示す参考実装です。最初のセンサ動作確認には、どちらも必要ありません。

### サンプルの命名規則

現在のサンプル名は、Arduino IDE上で対象ボードが分かるように`Board + Purpose`形式にしています（`UnoBasicMeasurement`、`M5Core2QuickStart`）。対応品番がSLT5006だけの間は、名前を短くするため品番を省略します。別の品番を追加するときに、全サンプルの名前を`Product + Board + Purpose`形式（例: `SLT5006UnoBasicMeasurement`、`SLT5009M5Core2QuickStart`）にそろえます。

このリポジトリの目的は次の2つです。

1. 初めて使う方が、SLT5006とArduinoの定格に合わせた評価回路にセンサを接続し、スケッチを書き込むだけで動作を確認できるようにする。
2. 動作を確認した後、同じドライバを自社システムへ組み込むために、通信手順、エラー処理、測定値の単位を確認できるようにする。

> これはソフトウェアのクイックスタートです。SLT5006のUARTは3.3 Vロジック、Arduino Uno R3は5 Vロジックなので、UARTの両方向にレベル変換を入れてください。電源と電源投入順序は製品資料に従ってください。他のArduinoボードでは、そのボードのUART電圧と入出力仕様に合わせて回路を設計してください。

対象は **SLT5006（TTL UART、独自バイナリプロトコル、1台接続）** です。RS-232CのSLT5005をArduinoのUARTへ直接接続しないでください。公式の[ラインアップ・電気的仕様](https://www.murata.com/ja-jp/products/sensor/soil/design-development/lineup)と[データシートダウンロード](https://www.murata.com/ja-jp/products/sensor/soil/design-development/dl)も確認してください。

## 最初に確認する安全事項

> 配線の接続や取り外しの前に、センサとArduinoの電源を切ってください。活線挿抜はしないでください。

- センサの電源電圧範囲は3.0～6.0 V、UART信号は3.3 Vロジックです。電源には、最大50 mAの動作電流に加えて、起動時と過渡時の電流も供給できるものを使ってください。
- Arduino Uno R3のI/Oは5 Vロジックです。センサとUnoの間では、TXとRXの両方向で3.3 Vと5 Vを変換してください。
- センサとArduinoのGNDを共通にしてください。ArduinoのI/Oピンからセンサへ給電しないでください。ボードの電源端子からセンサへ給電する場合は、電流容量を確認してください。
- 未使用の線は、ほかの線と短絡しないように1本ずつ絶縁し、どこにも接続しないでください。

詳しくは[配線ガイド](docs/wiring.md)を参照してください。

## インストール

Arduino Library Managerで公開された後は、**Murata Soil Sensor**を検索してインストールしてください。依存ライブラリとして`M5Unified`と`ArduinoJson`が表示されたら、一緒にインストールしてください。Unoのサンプルはこれらをインクルードしないため、ファームウェアには組み込まれません。

公開前のZIPを使う場合は、「スケッチ」→「ライブラリをインクルード」→「.ZIP形式のライブラリをインストール」でインストールします。ZIPからインストールすると、依存ライブラリが自動ではインストールされないことがあります。M5Stackで使う場合は、Library Managerから`M5Unified` 0.2.xと`ArduinoJson` 7.xもインストールしてください。ライブラリが不足していると、コンパイルエラーになり、必要なライブラリ名がエラーメッセージに表示されます。

## Uno／Nanoで必要なもの

- SLT5006 1台
- Arduino Uno R3またはClassic Nano（最初の動作確認用）
- Arduino IDE 2.x
- データ通信に対応したUSBケーブル（充電専用ケーブルは使えません）
- センサの定格を満たす電源
- プッシュプルのUART信号に対応したレベル変換回路（3.3 Vと5 Vを両方向に変換でき、電源オフ時の逆給電を防止できるもの）
- 配線材

外部のArduinoライブラリは不要です。Uno／Nanoに標準で付属する`SoftwareSerial`を使います。

`SLT5006`ドライバはこのリポジトリに含まれていますが、Arduino IDEから使うには、後述の手順でリポジトリのZIPをライブラリとしてインストールしてください。解凍したリポジトリの`examples/UnoBasicMeasurement/UnoBasicMeasurement.ino`を直接開いても、Arduino IDEはリポジトリ直下の`src/`を探さないため、`SLT5006.h: No such file or directory`というエラーになります。

## Uno R3での配線例

Uno R3のI/Oは5 Vロジック、SLT5006のUARTは3.3 Vロジックなので、UARTの両方向にレベル変換を入れます。UnoのD11（TX）から黄色線（センサRX）へは5 Vから3.3 Vへ、青線（センサTX）からUnoのD10（RX）へは3.3 Vから5 Vへ変換してください。レベル変換器はプッシュプルのUART信号に対応したものを使い、I2C専用の双方向MOSFETモジュールは使わないでください。

| SLT5006 | 機能 | Arduino／電源側 |
|---|---|---|
| RED | センサ電源 | 定格を満たす3.0～6.0 V電源 |
| BLACK | GND | Arduino GNDおよび電源GND |
| WHITE | Enable（Highで動作） | このクイックスタートではREDへ接続 |
| BLUE | センサTX（3.3 V） | レベル変換を経由してArduino D10（RX）へ接続 |
| YELLOW | センサRX（3.3 V） | Arduino D11（TX）からレベル変換を経由して接続 |
| GRAY / shield | シールド | 通信を安定させるため、GNDへの接続を推奨 |
| GREEN / ORANGE | 内部用 | 接続せず、個別に絶縁 |

UARTの設定は **9600 bps、データ長8ビット、パリティなし、ストップビット1（8N1）** です。TXとRXは交差させて接続します。

このサンプルでは、GPIOによるスタンバイ制御は実装していません。WHITEをArduinoのGPIOで制御する場合は、Enable端子のVIH/VIL、絶対最大定格、ArduinoのGPIO電圧、片側の電源が切れているときの注入電流を製品資料で確認し、その結果に応じてドライバやレベル変換回路を入れてください。

## クイックスタート

1. GitHubの「Code」→「Download ZIP」でこのリポジトリをダウンロードします。ZIPは解凍しないでください。
2. Arduino IDEのBoards Managerで`Arduino AVR Boards`をインストールし、Arduino IDEを再起動します。
3. 「スケッチ」→「ライブラリをインクルード」→「.ZIP形式のライブラリをインストール」を選び、ダウンロードしたZIPを指定します（[Arduino公式手順](https://support.arduino.cc/hc/en-us/articles/5145457742236-Add-libraries-to-Arduino-IDE)）。
4. センサは接続しないまま、「ファイル」→「スケッチ例」→「Murata Soil Sensor」→`UnoBasicMeasurement`を開きます。
5. 「ツール」から`Arduino Uno`と正しいPortを選び、Verify（チェックマーク）、Upload（右矢印）の順に実行します。Classic Nanoの場合は、Board、Processor／bootloader、Portを使用するボードに合わせてください（[Arduino公式の書き込み手順](https://support.arduino.cc/hc/en-us/articles/4733418441116-Upload-a-sketch-in-Arduino-IDE)）。
6. 書き込みが終わったらUSBケーブルを外し、Arduino、センサ、外部電源をすべてオフにします。
7. 定格に合わせた電源回路とレベル変換回路を含めて、上表のとおり配線します。極性が正しいこと、短絡がないことを確認してください。
8. 製品資料と使用する回路で決められた順序で、両側に給電します。電源オフ時の逆給電防止（powered-off protection）がない回路では、信号線をつないだまま、Arduinoだけ、またはセンサだけに先に給電しないでください。
9. シリアルモニタを **115200 bps** で開きます。

このリポジトリでは、特定の電源やレベル変換器の動作は確認していません。評価用の回路が、SLT5006とArduinoの定格と電源投入順序を満たしていることを確認してください。

正常に動作すると、最初にファームウェアのバージョンとシリアル番号、続いて測定値が表示されます。次の例のシリアル番号は架空の値です。

```text
Murata SLT5006 quick start
Firmware: 1.7.6
Serial number: 12345678

--- SLT5006 measurement ---
Temperature       : 25.8125 degC
VWC               : 20.0 %
Bulk EC           : 0.123 dS/m
...
```

土壌水分やECの値は、設置状態、培地、周囲の条件によって変わります。表示された値が妥当かどうかは、製品資料に記載された設置方法と評価条件に従って確認してください。

## M5Stack Core2クイックスタート

1. [M5Stack公式手順](https://docs.m5stack.com/en/arduino/arduino_board)に従い、設定の「追加のボードマネージャのURL」に`https://static-cdn.m5stack.com/resource/arduino/package_m5stack_index.json`を追加してから、Boards Managerで`M5Stack`をインストールします。
2. このライブラリと、上記の依存ライブラリをインストールします。
3. センサは接続しないまま、「ファイル」→「スケッチ例」→「Murata Soil Sensor」→`M5Core2QuickStart`を開きます。
4. ボードに`M5Core2`を選び、Verify、Uploadの順に実行します。
5. USBを外してすべての電源をオフにし、[M5Stack配線ガイド](docs/m5stack-wiring.ja.md)に従って配線します。Port A（GPIO32/33）とPort C（GPIO13/14）のどちらを使うかは、スケッチ冒頭の`SENSOR_PORT`で選択します。
6. 決められた順序で給電し、シリアルモニタを115200 bpsで開きます。

クイックスタートの画面では、項目名と単位の列を固定し、数値を右寄せでそろえて表示します。最初の測定に成功するまでは、数値の代わりに`--`を表示します。自動測定は、測定の開始時刻を基準に5秒ごとに始まります。測定に5秒を超える時間がかかった場合は、完了後に次の測定を開始します。画面下部のタッチボタンは、左からA、B、Cです。`Button A: manual measurement`を押すと、次の周期を待たずに測定を要求します。測定中に押した場合、要求は実行中の測定が終わるまで待機します。次の周期は、手動測定の開始時刻を基準に計算します。クイックスタートでは、BとCに機能を割り当てていません。

`Supply`はSLT5006が報告する供給電圧で、Core2のバッテリー電圧ではありません。背景色を分けた`Core2 battery (est.)`欄には、Core2のバッテリー残量の目安と電圧を表示します。この欄はセンサの測定とは別に、5秒ごとに更新します。残量%は目安で、正確な残容量ではありません。負荷や充電状態によって変動します。バッテリーの値を取得できない場合は`unavailable`と表示し、取得できなかったことをシリアルモニタにも出力します。AXP192搭載のCore2では、残量%を電池電圧から推定しています。電池が減っても、センサへの供給電圧は電源回路によって約5 Vに保たれることがあります。そのため、`Supply`からCore2の残量は判断できません。

動作を確認できたら、`M5Core2CloudLogger`でmicroSDへのCSV記録と、Google Sheetsへのアップロード（任意）を追加できます。[GASセットアップ](docs/gas-setup.ja.md)と[ロガー設定](docs/logger-setup.ja.md)に従って設定してください。設定はUSB経由で内蔵NVSへ保存します。NVSに設定がない場合は、microSDの設定を読み込みます。microSDには、ログと送信待ちのデータも保存します。このサンプルでは、NVSとmicroSDに保存した設定を暗号化していません。

## PlatformIO

このリポジトリには、Arduino UnoとM5Stack Core2向けのPlatformIO環境が含まれています。

```sh
pio run -e uno_basic
pio run -e uno_cooperative_polling
pio run -e core2_quick_start
pio run -e core2_cloud_logger
```

`UnoBasicMeasurement`を書き込むときは、センサをArduinoから外した状態で次のコマンドを実行します。

```sh
pio run -e uno_basic -t upload
```

書き込み後はUSBを外し、Arduino、センサ、外部電源をすべてオフにしてから、[配線ガイド](docs/wiring.md)に従って接続してください。USBを再接続するとArduino側に給電されるため、使用する回路の電源投入順序で決めたタイミングでのみ再接続してください。両側の電源が安定するまでは、電源オフ時の逆給電防止機能で保護するか、UARTの両方向をハイインピーダンスに保ってください。両側に給電され、USBのデータ接続が有効になったら、次のコマンドでモニタを開きます。

```sh
pio device monitor -b 115200
```

協調ポーリング例を書き込む場合は、`uno_basic`を`uno_cooperative_polling`に変えてください。ルートの`platformio.ini`と`pio/`内のラッパーは、`examples/`のスケッチをビルドして検証するためのものです。サンプル本体は`examples/`にあるので、参照や編集にはそちらのスケッチを使ってください。このリポジトリの検証用設定を、自分のアプリケーションへコピーする必要はありません。

自分のPlatformIOプロジェクトからこのライブラリを使う場合は、公開されたGitリポジトリを`lib_deps`に指定します。

```ini
[env:uno]
platform = atmelavr
board = uno
framework = arduino
monitor_speed = 115200
lib_deps =
  https://github.com/murata-sensor/soil-sensor-samples-arduino.git
```

PlatformIOの`.cpp`ファイルでは、`#include <Arduino.h>`の後に`#include <SLT5006.h>`を書きます。PlatformIO Registryに公開する前でも、Git URLまたはローカルフォルダを依存先に指定すれば使えます。ビルドを確認しているのは、Arduino Uno（Atmel AVR）とM5Stack Core2（ESP32）です。他のボードは別途検証してください。

## サンプル

### 最初の動作確認

- [`UnoBasicMeasurement`](examples/UnoBasicMeasurement/UnoBasicMeasurement.ino): 初心者向けの例です。センサ情報を1回読み出し、測定ごとに完了を待ち、完了から約5秒待って次の測定を始めます。
- [`M5Core2QuickStart`](examples/M5Core2QuickStart/M5Core2QuickStart.ino): Core2の画面とシリアルモニタに、センサ情報と測定値を表示する例です。一般土壌、ロックウール、ココピートの3種類のVWC換算値は、ファームウェアのバージョンにかかわらず画面に表示します。ココピート向けPore ECは、対応ファームウェアの場合だけ表示します。変換を待つ間もボタン処理とバッテリー表示の更新を続けるため、協調ポーリングを使っています。

### 応用例（任意）

- [`UnoCooperativePolling`](examples/UnoCooperativePolling/UnoCooperativePolling.ino): `UnoBasicMeasurement`と同じセンサ情報と測定項目を表示する応用例です。測定の開始、完了確認、値の取得を別々に呼び出し、変換を待つ間も内蔵LEDを点滅させます。ほかのループ処理を続けたい場合の参考にしてください。基本的なセンサの使い方を学ぶだけなら必要ありません。UART呼び出し自体で処理が止まる時間は後述します。
- [`M5Core2CloudLogger`](examples/M5Core2CloudLogger/M5Core2CloudLogger.ino): SD/CSV記録、Wi-Fi接続、Google Sheetsへの再送付きアップロードを示す参考実装です。ロガーの追加設定が必要で、最小限の測定例ではありません。

測定タイムアウトの既定値30秒は、変換の完了を待つ上限です。通常の測定にかかる時間ではありません。

## 取得できる値

| メンバー | 内容 | 単位 |
|---|---|---|
| `temperatureC` | 温度 | degC |
| `vwcPercent` | 一般土壌向け体積含水率 | % |
| `vwcRockPercent` | ロックウール向け体積含水率 | % |
| `vwcCocoPercent` | ココピート向け体積含水率 | % |
| `ecBulkDsM` | Bulk EC | dS/m |
| `ecPoreDsM` | Pore-water EC | dS/m |
| `ecPoreCocoDsM` | ココピート向けPore EC | dS/m |
| `batteryVoltageV` | センサが報告する電源電圧（換算値） | V |

ファームウェア1.7.5未満でもword 9は読み出せますが、値は0で、ココピート向けPore ECには対応していません。この値を表示・保存する前に、必ず`SLT5006::supportsEcPoreCoco(info)`で対応を確認してください。

公式の電気的仕様ページで公開されている代表的な計測範囲と分解能は次のとおりです。精度の条件、設置条件、人工培土向けの値の適用条件は、最新のデータシートを参照してください。

| 項目 | 計測範囲 | 分解能 |
|---|---|---|
| EC | 0～5 dS/m | 0.001 dS/m |
| 温度 | -20～60 degC | 0.0625 degC |
| VWC | 0～60%（一般土壌）、0～100%（人工培土） | 0.1% |

### 培地とフィールドを選ぶ

`UnoBasicMeasurement`は比較のために複数の換算値を表示しますが、これらを同時に得た独立の測定値として扱わないでください。システムでは、培地と目的に合ったフィールドだけを使ってください。

| 用途 | 主に確認するフィールド |
|---|---|
| 一般土壌の水分 | `vwcPercent` |
| ロックウールの水分 | `vwcRockPercent` |
| ココピートの水分 | `vwcCocoPercent` |
| Bulk EC | `ecBulkDsM` |
| Pore-water EC | `ecPoreDsM` |
| ココピート向けPore EC | `ecPoreCocoDsM`（対応ファームウェアと適用条件を確認） |

設置方法、培地別の換算の適用条件、仕様範囲外や飽和時の扱いは、最新のデータシートと公式の[推奨設置方法](https://www.murata.com/ja-jp/products/sensor/soil/design-development/institution)に従ってください。ファームウェアのバージョンとシリアル番号が表示され、CRCも正しければ、通信は確認できています。ただし、それだけでは測定値が妥当かどうかまでは確認できません。

## 自社システムへの組み込み

ドライバは`SoftwareSerial`に依存せず、Arduinoの`Stream`を受け取ります。実運用では、ハードウェアUARTがあるボードなら`Serial1`などを優先して使ってください。

```cpp
#include <Arduino.h>
#include <SLT5006.h>

SLT5006 sensor(Serial1);

void setup() {
  Serial1.begin(SLT5006::BAUD_RATE);
}

void loop() {
  SLT5006::Measurement value;
  if (sensor.readMeasurement(value)) {
    // value.vwcPercentなどを自社処理へ渡す
  } else {
    // sensor.lastError()を記録し、次周期で再試行する
  }
}
```

`readMeasurement()`は、測定の完了まで待つシンプルなAPIです。変換を待つ間にほかの処理を進める場合は、協調ポーリングAPIを次の順に呼び出します。

1. `startMeasurement()`
2. `measurementComplete(complete)`を繰り返し呼び出す
3. `complete == true`になったら`readMeasurementData()`を呼び出す

このAPIでも、UARTの送受信までは非同期になりません。測定開始、状態確認、18バイトの値取得はそれぞれ1回の要求と応答で、そのたびに呼び出し元を最大で応答タイムアウト1回分（既定値では約300 ms）止めます。状態確認と値取得を続けて呼ぶ場合は、両方の待ち時間を見込んでください。

詳しい状態管理、タイムアウト、再試行、プロトコル仕様は[組み込みガイド](docs/integration.md)と[プロトコル資料](docs/protocol.md)を参照してください。

## エラーが出たとき

| 表示 | 最初に確認すること |
|---|---|
| `SLT5006.h: No such file or directory` | GitHubからダウンロードしたZIPを「.ZIP形式のライブラリをインストール」でインストールし、「ファイル」→「スケッチ例」→「Murata Soil Sensor」からサンプルを開いてください。 |
| `M5Unified.h: No such file or directory` / `ArduinoJson.h: No such file or directory` | Library ManagerでM5Unified 0.2.x（M5GFXを含む）とArduinoJson 7.xをインストールし、Verifyをやり直してください。 |
| `No response from SLT5006 (timeout).` / `sensor response timed out` | 規定時間内にセンサから応答がありませんでした。センサを接続していなければ、この表示は想定どおりです。接続している場合は、すべての電源を切ってから、センサ電源、WHITE（Enable）、GNDの共通接続、BLUE→D10、YELLOW←D11、UARTのロジック電圧を確認してください。 |
| `sensor response CRC did not match` | ロジック電圧、9600 bpsの設定、配線長、接触、ノイズ、シールドのGND接続を確認してください。 |
| `sensor response format was invalid` | 接続しているのがSLT5006であること、1台だけ接続していることを確認してください。古い受信データやノイズが混ざっていないかも確認してください。 |
| `sensor returned a device error` | 続けて表示される16進のエラーコードと説明を記録してください。 |
| `measurement did not finish in time` | 電源の安定性とセンサの状態を確認し、次の周期で再試行してください。繰り返し発生する場合は記録を残してください。 |

エラーが出た後も、配線を変更する前には必ず電源を切ってください。

## 実装上の要点

- SLT5006は1対1接続専用です。複数台を同じUARTにつなぐためのアドレス機能はありません。
- CRCの計算はCRC-16/MODBUSと同じですが、SLT5006では **上位バイト、下位バイト** の順に送ります。一般的なMODBUS RTUの送信順とは逆です。
- 応答のCRCだけでなく、機能コード、レジスタアドレス、サイズ、書き込みACKのエコーも検証します。
- 応答バッファは最大31バイトの固定長です。動的メモリ確保と例外は使いません。
- タイムアウトの計算は、`millis()`の桁あふれに対応しています。

## 検証範囲

プロトコルと換算式は、SLT5006の製品資料に基づいて実装しています。ホスト単体テストでは、既知のフレーム、分割された応答、CRC・プロトコル・センサのエラー、測定値の変換、タイミング、`millis()`の桁あふれを確認しています。公開CIでは、Uno向けの2例とM5Stack Core2向けの2例をビルドしています。

この検証には、Arduino UnoまたはM5Stack Core2に実機のSLT5006を接続した試験は含まれていません。使用するボード、レベル変換回路、電源、センサの組み合わせで、連続測定、切断と復旧、乾湿変化を含めて実機で確認してください。

## 開発・リリース時の品質確認

公開CIでは、Linux、macOS、Windowsでホストテストを実行し、バージョンを固定したArduino Uno向け2例とM5Stack Core2向け2例をビルドします。コードの書式、静的解析、メタデータ、サンプルのパス、文書内のリンクも確認します。ロガーのJSON/CSV形式、SD書き込みの失敗、キューの検証、NVS/USB設定、Python送信ツール、TLS証明書、GASの受信検証もテストします。さらに、配布用ZIPをインストールした状態で、Arduino CLI 1.5.1、Arduino AVR Boards 1.8.6、M5Stack Boards 3.3.10を使って全4スケッチをビルドします。手元での実行方法は[ホストテストガイド（英語）](tests/README.md)を参照してください。

リリース時は`library.properties`と`library.json`のバージョンを一致させ、[GitHub Releases](https://github.com/murata-sensor/soil-sensor-samples-arduino/releases)にリリースノートを作成します。一般的な問題は[CONTRIBUTING.md](CONTRIBUTING.md)、脆弱性は[SECURITY.md](SECURITY.md)の手順に従って報告してください。

## ライセンス

BSD-3-Clauseです。詳細は[LICENSE](LICENSE)を参照してください。このサンプルは、評価・参考用として現状のまま提供されます。実運用の前に、該当製品の最新資料で仕様と動作を確認してください。
