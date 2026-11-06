# システム組み込みガイド

[English](integration.en.md) | 日本語

## 通信ポートを選ぶ

`SLT5006`は`Stream&`を受け取るため、`SoftwareSerial`、`Serial1`、互換UARTクラスを使用できます。

```cpp
SLT5006 sensor(Serial1);

void setup() {
  Serial1.begin(SLT5006::BAUD_RATE);  // 9600 bps, 8N1
}
```

実運用ではハードウェアUARTを優先してください。`SoftwareSerial`は簡単な動作確認に便利ですが、次の制約があります。

- 同時に受信待ちできるSoftwareSerialは1つだけ
- ボードによってRXに使えるピンが異なる
- 送受信中の割込み制約がServo、タイマ処理、別ソフトUART等へ影響する場合がある
- 高負荷のメインループでは受信取りこぼしを評価する必要がある

1つのUARTを複数タスクや割込み処理から同時に操作しないでください。SLT5006は1台接続専用です。

独自`Stream`を使用する場合、`flush()`は現在のArduino仕様どおり、未送信dataの送信
完了を待つか、同期書込みなら何もせず戻る実装にしてください。受信済みdataを破棄
してはいけません。また、`available()`確認直後に別処理がbyteを消費して`read()`が
`-1`を返す場合でも、timeout処理が進行できる実装にしてください。

運用中の片系電源断も設計対象です。Arduinoが動作したままセンサ電源だけが失われる場合、TXを駆動し続けると信号線から逆給電する可能性があります。センサ未給電時はUART出力をhigh-impedanceにする、powered-off protection付き変換器で分離する、または製品資料で承認された同等対策を入れてください。再起動・保守・コネクタ抜け・電源故障も含めて確認します。

## Blocking API

簡単な周期測定では`readMeasurement()`を使用します。この関数は測定開始、完了待ち、値読出しまでを行います。

```cpp
SLT5006::Measurement value;
if (sensor.readMeasurement(value)) {
  saveValue(value.vwcPercent);
} else {
  logError(sensor.lastError());
}
```

既定では、開始ACK後300 ms待ち、200 ms間隔で状態を確認し、開始ACK後の経過時間が30秒に達したら新しい状態確認を開始しません。待機中にアプリケーションの他処理を行う必要がある場合は協調ポーリングAPIを使用します。

## 協調ポーリングAPI

状態遷移は次のとおりです。

```text
IDLE --startMeasurement()--> MEASURING
MEASURING --measurementComplete(false)--> MEASURING
MEASURING --measurementComplete(true)--> READY
READY --readMeasurementData()--> IDLE
```

`measurementComplete()`は、開始後300 msまではUART要求を送らず`complete=false`を返します。その後も設定したpoll間隔より短い呼出しはUART要求を送らないため、メインループから繰り返し呼べます。完全な例は[`UnoNonBlockingMeasurement`](../examples/UnoNonBlockingMeasurement/UnoNonBlockingMeasurement.ino)を参照してください。

これは最大30秒の測定変換待ちを協調化するAPIで、UART受信まで完全に非同期化するものではありません。`startMeasurement()`、pollが必要な`measurementComplete()`、18 byteを読む`readMeasurementData()`はそれぞれ1取引で、最大1回分のresponse timeout（既定値では約300 ms）だけ呼出し元を停止する可能性があります。状態確認と値取得を続けて呼ぶ場合は両方の待ち時間を見込みます。厳密な応答性が必要な場合は、独自の非同期UART状態機械、RTOS task、または専用通信処理へ分離してください。

順序外で`readMeasurementData()`を呼ぶと`MEASUREMENT_NOT_READY`になります。通信エラー時は状態を破棄し、間隔を置いて`startMeasurement()`から再試行してください。

## タイムアウト

```cpp
sensor.setResponseTimeout(300);      // 1応答の受信待ち [ms]
sensor.setPollInterval(200);         // 状態確認の最小間隔 [ms]
sensor.setMeasurementTimeout(30000); // 測定変換待ち期限 [ms]
```

既定値はこのサンプルで使用する基準値です。短く変更すると正常な遅延をエラーと判定する可能性があります。長く変更すると断線検出が遅れます。変更後は、実際のボード、UART、配線、センサ、温度範囲で評価してください。

タイムアウト計算は符号なし差分を用い、`millis()`が最大値から0へ戻る場合にも対応します。

`measurementTimeout`は厳密なAPI wall-clock上限ではなく、開始ACK後に次の状態確認を開始できる変換待ち期限です。期限直前に開始したUART応答待ちと、blocking APIのpoll間隔待ちは期限を越える場合があります。厳密な処理期限があるシステムでは個々の`responseTimeout`を含めて余裕を設けるか、上位のdeadline管理を追加してください。

## エラー処理と再試行

全APIは成功時に`true`、失敗時に`false`を返します。

```cpp
if (!sensor.readMeasurement(value)) {
  Serial.println(sensor.errorMessage());
  if (sensor.lastError() == SLT5006::DEVICE_ERROR) {
    Serial.println(sensor.lastDeviceErrorCode(), HEX);
    Serial.println(sensor.deviceErrorMessage());
  }
}
```

推奨方針:

1. エラー種別、センサエラーコード、時刻を記録する。
2. 同じ周期内で要求を連打せず、次の測定周期まで待って再試行する。
3. 連続失敗時は電源、GND、UARTレベル、配線、ノイズを確認する。
4. 配線を変更する前に必ず電源を切る。

測定開始ACK後にセンサ内部の測定が失敗した場合、ACKとは別に遅延エラーフレームが届く場合があります。次取引前に入力を再同期し、後続のdevice error、format error、またはmeasurement timeoutを測定失敗として扱ってください。本ドライバは各要求前に残留受信データを破棄し、次の要求／次周期から再同期します。

## Firmware機能差

センサ情報を起動時に読み、Pore EC coco peatの使用可否を保存します。

```cpp
SLT5006::SensorInfo info;
bool hasCocoPoreEc = false;
if (sensor.readInfo(info)) {
  hasCocoPoreEc = SLT5006::supportsEcPoreCoco(info);
}
```

- firmware 1.6.3: `ecPoreCocoDsM`非対応
- firmware 1.7.5、1.7.6: 対応

非対応firmwareでもword 9は読み出せますが、値は0であり、対応測定値ではありません。非対応またはfirmwareを識別できなかった場合、値を表示、保存、アップロードしないでください。他の測定値は個別の製品仕様に従って評価してください。

## メモリとデータ受渡し

- ドライバは動的`String`、ヒープ確保、例外を使用しません。
- 最大受信フレーム用に31 byteの固定配列を使用します。
- `Measurement`は顧客システムで使う換算済みの`float`値だけを公開します。
- ECはdS/m、VWCは%、温度はdegC、電圧はVです。保存先の列名にも単位を含めることを推奨します。
- Arduino Unoの`double`は`float`と同じ32-bitです。より高い精度が必要な場合は、対象ボードとデータ形式を含めて別途要件を評価してください。

## 測定値の選択と妥当性

通信成功と測定妥当性を別々に判定します。

1. Firmware、Serial、CRC-validな応答で通信経路を確認する。
2. 使用培地に対応するVWCフィールドを1つ選ぶ（一般土壌=`vwcPercent`、rock wool=`vwcRockPercent`、coco peat=`vwcCocoPercent`）。
3. 必要な物理量に応じてBulk ECとPore-water ECを区別する。
4. `ecPoreCocoDsM`はfirmware対応だけでなく、製品資料に記載された適用条件も確認する。
5. 公式の[推奨設置方法](https://www.murata.com/ja-jp/products/sensor/soil/design-development/institution)に従い、既知の培地・設置条件で変化と再現性を確認する。
6. 仕様範囲外、飽和、異常値の扱いをシステム要件として定義する。ドライバは受信値を仕様範囲へclampしない。

複数の培地向け換算値を、同時に得られた独立測定値として保存・平均しないでください。

## 移植・独自実装時の確認事項

独自ドライバへ移植する場合は[プロトコル資料](protocol.md)に加え、特に次をテストしてください。

- CRC計算値とCRCのhigh-byte-first送信順
- 分割して到着する応答の再構成
- 4-byteエラー応答と通常応答の長さ判定
- CRC、機能コード、address、size、write echoの不一致拒否
- signed 12-bit温度とlittle-endian値
- 測定開始後300 msより前に状態要求を送らないこと
- 30秒の測定タイムアウトと`millis()` wrap-around
