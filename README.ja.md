# soil-sensor-samples-arduino

[English (default)](README.md) | 日本語

村田製作所の土壌センサ **SLT5006** をArduino Uno／NanoおよびM5Stack Core2から動かすためのサンプルと、再利用可能なUARTドライバです。

| Platform | 最初に使うサンプル | 追加機能 |
|---|---|---|
| Arduino Uno / Classic Nano | `UnoBasicMeasurement` | `UnoNonBlockingMeasurement` |
| M5Stack Core2 | `M5Core2QuickStart` | `M5Core2CloudLogger` |

### サンプルの命名規則

現在はArduino IDE上で対象boardが分かるよう、`Board + Purpose`形式の
`UnoBasicMeasurement`、`M5Core2QuickStart`という名前を使用します。対応品番が
SLT5006だけの間は、名前を短くするため品番を省略します。別品番を追加する時点で、
全サンプルを`Product + Board + Purpose`形式へ揃えます。例:
`SLT5006UnoBasicMeasurement`、`SLT5009M5Core2QuickStart`。

このリポジトリには2つの目的があります。

1. 初めて使う方が、定格適合を回路設計者が確認した評価回路へ接続してスケッチを書き込むだけでセンサの動作を確認する。
2. 動作確認後、同じドライバを自社システムへ組み込むための通信手順、エラー処理、測定値の単位を確認する。

> これはソフトウェアのクイックスタートです。SLT5006のUARTロジック電圧と電源シーケンスは使用するArduino・変換回路によって条件が変わるため、評価回路は製品資料を確認できる回路設計者が準備または承認してください。回路が準備できれば、初心者向け例はプログラム変更なしで実行できます。

対象は **SLT5006（TTL UART、独自バイナリプロトコル、1台接続）** です。RS-232CのSLT5005をArduinoのUARTへ直接接続しないでください。公式の[ラインアップ・電気的仕様](https://www.murata.com/ja-jp/products/sensor/soil/design-development/lineup)と[データシートダウンロード](https://www.murata.com/ja-jp/products/sensor/soil/design-development/dl)も確認してください。

## 最初に確認する安全事項

> 配線の接続・取り外しは、センサとArduinoの電源を切った状態で行ってください。活線挿抜は禁止です。

- センサの電源範囲は3.0～6.0 V、公開仕様上の動作電流は最大50 mA（Read/Write cycle）です。起動・過渡条件を含む必要電流、ピン配置、UARTロジック電圧は、使用するSLT5006の最新データシート／取扱説明書で確認してください。
- Arduino UnoのTXは5 Vロジックです。SLT5006のRXが5 V tolerantであるとは仮定しないでください。ロジック電圧が一致しない場合は、UART push-pull信号に対応する方向別のレベル変換回路を使用します。
- センサをArduinoの5 V端子から給電できるとは仮定しません。定格と必要電流を満たす電源を使用し、センサとArduinoのGNDを共通にします。
- 未使用線は他の線と短絡しないよう、1本ずつ絶縁してフローティングにします。

詳しくは[配線ガイド](docs/wiring.md)を参照してください。

## インストール

Arduino Library Managerで公開後は、**Murata Soil Sensor**を検索して導入します。
表示される`M5Unified`と`ArduinoJson`の依存関係も同時に導入してください。Unoの
サンプルではこれらをincludeしないため、firmwareには組み込まれません。

公開前のZIPを使う場合は「スケッチ」→「ライブラリをインクルード」→
「.ZIP形式のライブラリをインストール」で導入します。ZIPでは依存関係が自動導入
されない場合があるため、M5Stack利用時はLibrary Managerから`M5Unified` 0.2.xと
`ArduinoJson` 7.xも導入してください。不足時はサンプルが必要なlibrary名をcompile
errorとして表示します。

## Uno／Nanoで必要なもの

- SLT5006 1台
- Arduino Uno R3またはClassic Nano（最初の動作確認例）
- Arduino IDE 2.x
- データ通信対応USBケーブル（充電専用ケーブルは使用不可）
- センサ定格を満たす電源
- 必要に応じてUART push-pull信号用のレベル変換回路（TX/RXそれぞれの方向に対応し、電源OFF時の逆給電を防止できるもの）
- 配線材

外部Arduinoライブラリは不要です。Uno/Nanoの標準`SoftwareSerial`を使用します。

`SLT5006`ドライバはこのリポジトリに同梱されていますが、Arduino IDEから使用するには、下記の手順でリポジトリのZIPをライブラリとしてインストールする必要があります。解凍したリポジトリ内の`examples/UnoBasicMeasurement/UnoBasicMeasurement.ino`を直接開くだけでは、Arduino IDEがリポジトリ直下の`src/`を探索しないため、`SLT5006.h: No such file or directory`になります。

## Uno R3での配線例

以下は信号の対応関係です。ロジック電圧が一致しない場合、青線／黄線とArduinoの間にUART push-pull信号対応のレベル変換回路を方向ごとに入れてください。I2C専用の双方向MOSFETモジュールをUARTにも使用できるとは仮定しません。

| SLT5006 | 機能 | Arduino／電源側 |
|---|---|---|
| RED | センサ電源 | 定格を満たす3.0～6.0 V電源 |
| BLACK | GND | Arduino GNDおよび電源GND |
| WHITE | Enable（Highで動作） | このクイックスタートではREDへ接続 |
| BLUE | センサTX | Arduino D10（RX）へ |
| YELLOW | センサRX | Arduino D11（TX）から |
| GRAY / shield | シールド | 安定通信のためGND接続を推奨 |
| GREEN / ORANGE | 内部用 | 接続せず、個別に絶縁 |

UARTは **9600 bps、8 data bits、parityなし、1 stop bit（8N1）** です。TXとRXは交差させます。

WHITEをArduino GPIOで制御する場合も、Enable端子のVIH/VIL、絶対最大定格、ArduinoのGPIO電圧、片側電源OFF時の注入電流を製品資料で確認し、必要なドライバ／レベル変換を使用してください。このサンプルはGPIOによるstandby制御を実装していません。

## クイックスタート

1. GitHubの「Code」→「Download ZIP」でこのリポジトリをダウンロードします。ZIPは解凍しません。
2. Arduino IDEのBoards Managerで`Arduino AVR Boards`を導入し、Arduino IDEを再起動します。
3. 「スケッチ」→「ライブラリをインクルード」→「.ZIP形式のライブラリをインストール」を選び、ZIPを指定します（[Arduino公式手順](https://support.arduino.cc/hc/en-us/articles/5145457742236-Add-libraries-to-Arduino-IDE)）。
4. センサをまだ接続せず、「ファイル」→「スケッチ例」→「Murata Soil Sensor」→`UnoBasicMeasurement`を開きます。
5. 「ツール」から`Arduino Uno`と正しいPortを選び、まずVerify（チェックマーク）、次にUpload（右矢印）を実行します。Classic Nanoの場合はBoard、Processor／bootloader、Portを使用基板に合わせます（[Arduino公式書込み手順](https://support.arduino.cc/hc/en-us/articles/4733418441116-Upload-a-sketch-in-Arduino-IDE)）。
6. 書込み後にUSBケーブルを外し、Arduino、センサ、外部電源をすべてOFFにします。
7. 承認済みの電源・レベル変換回路を含めて上表のとおり配線し、短絡と極性を確認します。
8. 製品資料と使用回路で規定された順序で両側を給電します。powered-off protectionのない回路で、Arduinoだけ、またはセンサだけを信号線接続のまま先に給電しないでください。
9. シリアルモニタを **115200 bps** で開きます。

このリポジトリは特定の電源やレベル変換器を認定するものではありません。評価用ハードウェアは、SLT5006とArduino双方の定格および電源シーケンスを満たすことを回路設計者が確認してください。

正常時は、最初にファームウェア版とシリアル番号、その後に測定値が表示されます。
以下のシリアル番号は架空の値です。

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

土壌水分やECは設置状態・培地・周囲条件に依存します。表示された値の妥当性は、製品資料に記載された設置方法と評価条件に従って確認してください。

## M5Stack Core2クイックスタート

1. [M5Stack公式手順](https://docs.m5stack.com/en/arduino/arduino_board)に従い、
  設定の「追加のボードマネージャのURL」へ
  `https://static-cdn.m5stack.com/resource/arduino/package_m5stack_index.json`
  を追加してから、Boards Managerで`M5Stack`を導入します。
2. 本libraryと上記依存libraryを導入します。
3. センサを接続せず、「ファイル」→「スケッチ例」→「Murata Soil Sensor」→`M5Core2QuickStart`を開きます。
4. Board=`M5Core2`を選択し、Verify後にUploadします。
5. USBを外して全電源をOFFにし、[M5Stack配線ガイド](docs/m5stack-wiring.ja.md)に従って配線します。
  Port A（GPIO32/33）とPort C（GPIO13/14）はスケッチ冒頭の`SENSOR_PORT`で選択します。
6. 承認済みの順序で給電し、シリアルモニタを115200 bpsで開きます。

クイックスタート画面は項目名と単位の列を固定し、数値を右寄せでそろえて表示します。
最初の測定に成功するまでは、数値の代わりに`--`を表示します。
自動測定は測定開始を基準に5秒周期です。測定が5秒を超えた場合は完了後に次を開始します。
画面下部のタッチボタンは左からA・B・Cです。`Button A: manual measurement`は、
次の周期を待たずに測定を要求します。測定中は完了を待ち、手動測定の開始を基準に
次の周期を計算します。クイックスタートではB・Cに操作を割り当てていません。
`Supply`はSLT5006が報告する供給電圧で、Core2のバッテリー電圧ではありません。
背景色を分けた`Core2 battery (est.)`欄にはCore2のバッテリー残量の目安と電圧を表示し、
センサ測定とは独立して5秒ごとに更新します。残量%は正確な残容量ではなく目安であり、
負荷や充電状態で変動します。取得できない場合は`unavailable`と表示し、
シリアルモニタにも取得できなかったことを出力します。
AXP192搭載Core2の残量%は電池電圧からの推定です。電池が減ってもセンサへの供給電圧は
電源回路によって約5Vに維持され得るため、`Supply`からCore2の残量は判断できません。

動作確認後、`M5Core2CloudLogger`でmicroSDへのCSV記録とGoogle Sheetsへの任意uploadを
追加できます。[GASセットアップ](docs/gas-setup.ja.md)と
[ロガー設定](docs/logger-setup.ja.md)に従ってください。設定はUSB経由で内蔵NVSへ保存し、
NVS未設定時はmicroSDの設定を読み込みます。microSDにはログ・送信待ちデータも保存します。
NVS・SD設定はこのサンプルでは暗号化していません。

## PlatformIO

このリポジトリにはArduino UnoとM5Stack Core2向けのPlatformIO環境が含まれています。

```sh
pio run -e uno_basic
pio run -e uno_nonblocking
pio run -e core2_quick_start
pio run -e core2_cloud_logger
```

センサをArduinoから外した状態で`UnoBasicMeasurement`を書き込む場合:

```sh
pio run -e uno_basic -t upload
```

書込み後はUSBを外し、Arduino、センサ、外部電源をすべてOFFにしてから[配線ガイド](docs/wiring.md)に従って接続してください。USBの再接続もArduino側への給電操作として扱い、承認済み回路で定めた給電順序の時点でのみ再接続します。両側の電源が安定するまでpowered-off protectionまたはUART両方向のhigh-impedanceが必要です。両側が給電されUSBデータ接続が有効になった後、次のコマンドでモニタを開きます。

```sh
pio device monitor -b 115200
```

協調ポーリング例を書き込む場合は`uno_basic`を`uno_nonblocking`へ変更します。ルートの`platformio.ini`と`pio/`内のwrapperは、`examples/`にあるスケッチをビルドするための検証用です。参照・編集するサンプル本体は`examples/`にあり、リポジトリの検証設定を利用者のアプリケーションへコピーする必要はありません。

自分のPlatformIOプロジェクトからライブラリを使う場合は、公開後のGitリポジトリを`lib_deps`へ指定します。

```ini
[env:uno]
platform = atmelavr
board = uno
framework = arduino
monitor_speed = 115200
lib_deps =
  https://github.com/murata-sensor/soil-sensor-samples-arduino.git
```

PlatformIOの`.cpp`ファイルでは、`#include <Arduino.h>`に続けて`#include <SLT5006.h>`を記述します。PlatformIO Registryへ公開する前でも、Git URLまたはローカルフォルダ依存として利用できます。ビルド確認対象はArduino Uno／Atmel AVRとM5Stack Core2／ESP32です。他のボードは別途検証してください。

## サンプル

- [`UnoBasicMeasurement`](examples/UnoBasicMeasurement/UnoBasicMeasurement.ino): センサ情報を1回読み、各測定の完了後に約5秒待って次を測定する初心者向け例。
- [`UnoNonBlockingMeasurement`](examples/UnoNonBlockingMeasurement/UnoNonBlockingMeasurement.ino): 既定30秒の測定変換待ちを協調ポーリングにし、その間もLED制御などを続ける組み込み例。UART呼出し自体の停止時間は後述します。
- [`M5Core2QuickStart`](examples/M5Core2QuickStart/M5Core2QuickStart.ino): Core2画面とSerial Monitorにセンサ情報と測定値を表示する例。
- [`M5Core2CloudLogger`](examples/M5Core2CloudLogger/M5Core2CloudLogger.ino): SD/CSV、Wi-Fi、Google Sheetsへの再送付きuploadを追加する例。

## 取得できる値

| メンバー | 内容 | 単位 |
|---|---|---|
| `temperatureC` | 温度 | degC |
| `vwcPercent` | 体積含水率 | % |
| `vwcRockPercent` | ロックウール向け体積含水率 | % |
| `vwcCocoPercent` | ココピート向け体積含水率 | % |
| `ecBulkDsM` | Bulk EC | dS/m |
| `ecPoreDsM` | Pore-water EC | dS/m |
| `ecPoreCocoDsM` | ココピート向けPore EC | dS/m |
| `batteryVoltageV` | センサが報告する電源電圧換算値 | V |

ファームウェア1.7.5未満でもword 9は読み出せますが、値は0で、ココピート向けPore ECの測定値としては非対応です。必ず`SLT5006::supportsEcPoreCoco(info)`を確認してから表示・保存してください。

公式の電気的仕様ページで公開されている代表範囲・分解能は次のとおりです。精度条件、設置条件、人工培土向け値の適用条件は最新データシートを参照してください。

| 項目 | 計測範囲 | 分解能 |
|---|---|---|
| EC | 0～5 dS/m | 0.001 dS/m |
| Temperature | -20～60 degC | 0.0625 degC |
| VWC | 0～60%（一般土壌）、0～100%（人工培土） | 0.1% |

### 培地とフィールドを選ぶ

`UnoBasicMeasurement`は比較確認のため複数の換算値を表示しますが、これらを同時に得られた独立測定値として扱わないでください。システムでは培地と目的に対応するフィールドだけを採用します。

| 用途 | 主に確認するフィールド |
|---|---|
| 一般土壌の水分 | `vwcPercent` |
| ロックウールの水分 | `vwcRockPercent` |
| ココピートの水分 | `vwcCocoPercent` |
| Bulk EC | `ecBulkDsM` |
| Pore-water EC | `ecPoreDsM` |
| ココピート向けPore EC | `ecPoreCocoDsM`（対応firmwareと適用条件を確認） |

設置方法、培地別換算の適用条件、仕様範囲外や飽和時の扱いは、最新データシートと公式の[推奨設置方法](https://www.murata.com/ja-jp/products/sensor/soil/design-development/institution)に従ってください。Firmware/Serial/CRCが読めれば「通信」は確認できますが、それだけで測定値の妥当性まで確認できたことにはなりません。

## 自社システムへの組み込み

ドライバは`SoftwareSerial`に依存せず、Arduinoの`Stream`を受け取ります。ハードウェアUARTを持つボードでは、実運用時に`Serial1`などを優先してください。

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

`readMeasurement()`は完了まで待つ簡単なAPIです。長い測定変換待ちの間にも他処理を進める場合は、協調ポーリングAPIを次の順で使用します。

1. `startMeasurement()`
2. `measurementComplete(complete)`を繰り返す
3. `complete == true`なら`readMeasurementData()`

このAPIはUART受信まで完全に非同期化するものではありません。測定開始、状態確認、18 byteの値取得はそれぞれ1取引で、最大1回分のresponse timeout（既定値では約300 ms）だけ呼出し元を停止する可能性があります。状態確認と値取得を続けて呼ぶ場合は、両方の待ち時間を見込んでください。

詳しい状態管理、タイムアウト、再試行、プロトコル仕様は[組み込みガイド](docs/integration.md)と[プロトコル資料](docs/protocol.md)を参照してください。

## エラーが出たとき

| 表示 | 最初に確認すること |
|---|---|
| `SLT5006.h: No such file or directory` | GitHubからダウンロードしたZIPを「.ZIP形式のライブラリをインストール」で導入し、「ファイル」→「スケッチ例」→「Murata Soil Sensor」からサンプルを開く |
| `M5Unified.h: No such file or directory` / `ArduinoJson.h: No such file or directory` | Library ManagerでM5Unified 0.2.x（M5GFXを含む）とArduinoJson 7.xを導入し、Verifyを再実行する |
| `No response from SLT5006 (timeout).` / `sensor response timed out` | 規定時間内にセンサから応答がなかったことを示す。センサ未接続なら想定どおり。接続済みなら、すべての電源を切ってから、センサ電源、WHITE（Enable）、GND共通、BLUE→D10、YELLOW←D11、UARTロジック電圧を確認する |
| `sensor response CRC did not match` | ロジック電圧、9600 bps、配線長、接触、ノイズ、シールドGND |
| `sensor response format was invalid` | SLT5006であること、1台だけ接続していること、古い受信データやノイズ |
| `sensor returned a device error` | 続けて表示される16進エラーコードと説明 |
| `measurement did not finish in time` | 電源の安定性、センサ状態。次周期で再試行し、継続する場合は記録を取得 |

エラー後も配線を変更する前に必ず電源を切ってください。

## 実装上の要点

- SLT5006は1対1専用です。複数台を同じUARTへ接続するアドレス機能はありません。
- CRCはCRC-16/MODBUSと同じ計算ですが、SLT5006では **上位バイト、下位バイト** の順で送ります。一般的なMODBUS RTUの送信順とは逆です。
- 応答のCRCだけでなく、機能コード、レジスタアドレス、サイズ、書込みACKのエコーも検証します。
- 応答バッファは最大31 byteの固定長で、動的メモリと例外を使用しません。
- タイムアウト計算は`millis()`の桁あふれに対応しています。

## 検証範囲

プロトコルと換算式はSLT5006の製品資料に基づいて実装しています。ホスト単体テストでは、既知フレーム、分割応答、CRC・プロトコル・センサエラー、測定値変換、タイミング、`millis()`桁あふれを確認します。公開CIではUno向け2例とM5Stack Core2向け2例をビルドします。

この検証に実際のArduino UnoまたはM5Stack Core2とSLT5006を接続したHardware-in-the-Loop試験は含みません。実際に使用するボード、レベル変換回路、電源、センサの組み合わせで、連続測定、切断・復旧、乾湿変化を含む実機確認を行ってください。

## 開発・release時の品質確認

公開CIでは、Linux／macOS／Windowsのhost test、version固定したArduino Uno向け
2例とM5Stack Core2向け2例のbuild、format、static analysis、metadata、example path、
文書内linkを確認します。ロガーのJSON/CSV形式、SD書込み失敗、キュー検証、
NVS/USB設定、Python送信ツール、TLS証明書、GAS受信検証もテストします。
local実行方法は[host test guide](tests/README.md)を参照してください。
配布用ZIPの導入後にも、Arduino CLI 1.5.1、Arduino AVR Boards 1.8.6、
M5Stack Boards 3.3.10で全4スケッチをビルドします。

release時は`library.properties`と`library.json`のversionを一致させ、
[GitHub Releases](https://github.com/murata-sensor/soil-sensor-samples-arduino/releases)にrelease notesを作成します。一般的な報告は
[CONTRIBUTING.md](CONTRIBUTING.md)、脆弱性は[SECURITY.md](SECURITY.md)に従って
ください。

## License

BSD-3-Clause。詳細は[LICENSE](LICENSE)を参照してください。本サンプルは評価・参考用に現状のまま提供されます。実運用前に該当製品の最新資料で仕様と動作を確認してください。
