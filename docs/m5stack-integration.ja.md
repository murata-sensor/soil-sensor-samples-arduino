# システム組み込みガイド

[English](m5stack-integration.md) | 日本語

最初の動作確認には`M5Core2QuickStart`、ロガー評価には`M5Core2CloudLogger`を使います。
量産・自社システムへ組み込むときは、UI、保存、通信、認証を分離し、次の境界を
明示してください。

```text
SLT5006 UART driver -> Measurement -> display / local CSV / upload queue -> GAS
```

## Arduino版との互換性

M5Core2版SLT5006ドライバはArduino版と同じ`Stream`ベースAPIとデータ型を使用します。
M5Core2ではUART2を初期化して渡します。下記は本体PORT.Aの`RX=32, TX=33`です。
対応BottomのPORT.Cを使う場合は`RX=13, TX=14`へ変更します。

```cpp
HardwareSerial sensorSerial(2);
SLT5006 sensor(sensorSerial);

void setup() {
  sensorSerial.begin(SLT5006::BAUD_RATE, SERIAL_8N1, 32, 33);
}
```

簡単な処理は`readMeasurement()`、画面やネットワークを進めながら測定変換を待つ
場合は次の協調APIを使います。

```text
IDLE --startMeasurement()--> MEASURING
MEASURING --measurementComplete(false)--> MEASURING
MEASURING --measurementComplete(true)--> READY
READY --readMeasurementData()--> IDLE
```

協調APIは測定変換待ちを分割しますが、各UART応答待ちまで完全非同期にはしません。
同じ`Serial2`を複数task、割込み、別ライブラリから同時に操作せず、1つの所有者へ
集約します。詳しいタイミングとフレームは[プロトコル資料](m5stack-protocol.ja.md)を参照
してください。

## 値と単位を境界で固定する

| C++ | 保存／JSON | 単位 |
|---|---|---|
| `batteryVoltageV` | `battery_v` | センサが報告する電源 [V] |
| Core2 power API | `core2_battery_v` | Core2内蔵電池 [V] |
| `temperatureC` | `temperature_c` | degC |
| `vwcPercent` | `vwc_pct` | %（一般土壌） |
| `vwcRockPercent` | `vwc_rock_pct` | %（ロックウール） |
| `vwcCocoPercent` | `vwc_coco_pct` | %（ココピート） |
| `ecBulkDsM` | `ec_bulk_dsm` | dS/m |
| `ecPoreDsM` | `ec_pore_dsm` | dS/m |
| `ecPoreCocoDsM` | `ec_pore_coco_dsm` | dS/m（firmware対応時のみ） |

単位を列名・API名へ含め、暗黙の変換を避けます。通信成功と測定値の妥当性は別です。
用途に対応する培地のVWCを1つ選び、設置方法、仕様範囲、飽和、異常値の扱いを
システム要件として定義してください。ドライバは受信値を範囲へclampしません。

起動時にFirmwareとSerial numberを読みます。firmware 1.7.5未満でもword 9は
読み出せますが、値は0であり対応測定値ではありません。`ec_pore_coco_dsm`は
firmware 1.7.5以降だけで保存・表示し、それ以外や識別失敗時は欠損値にします。

## タイムアウトと再試行

既定値はUART応答300 ms、状態poll 200 ms、測定全体30秒です。開始ACK後の最初の
pollまで最低300 ms待ちます。短縮すると正常な遅延を故障と誤認し、延長すると断線
検知が遅れます。変更後は実際のセンサ、配線、温度、処理負荷で評価してください。

推奨エラー処理:

1. error種別、センサerror code、時刻、連続回数を記録する。
2. 同じ要求を連打せず、入力を再同期して次の測定周期から再試行する。
3. CRC／format errorでは電源、GND、ロジックレベル、ノイズ、配線長を確認する。
4. 連続失敗へ指数backoffと上限を設け、watchdog reset loopを避ける。
5. 配線変更は必ず全電源OFFで行う。

`millis()`の経過時間は符号なし減算を使い、wrap-aroundに対応します。厳密な
deadlineがある場合、変換期限に加えて開始・poll・最終読出しそれぞれのUART
response timeoutを上位設計へ含めます。

## ローカル保存を先に確定する

クラウド失敗で測定値を失わないよう、1測定を次の順で扱います。

1. 名前付きrecordを作り、再送中に変化しない一意な`id`を付ける。
2. 最大2048 bytesのrecordを一時ファイルへ全バイト書き込み、flush／close後に
   renameしてqueueへ保存する。既存IDを上書きしない。
3. 同じrecordをCSVへappendし、ヘッダと行の書込みバイト数を確認してflush／closeする。
4. HTTPS送信し、最終responseのHTTP 2xxとJSON `ok:true`の両方を確認する。
5. 成功確認後だけqueue recordを削除する。

queueとCSVは個別に成否を確認し、クラウド配送を保全するqueueを優先します。
一時ファイルとrenameだけで、すべてのカードの電源断耐性を保証できるわけではありません。
CSVへの記録とqueue作成の途中で電源断した場合も考え、起動時に不完全ファイルを
検出・隔離してください。容量上限、保持期間、backup、カード摩耗、時刻未同期、
CSV injectionを含む外部入力のescapingを設計します。

同じ測定をCSVとクラウドで関連付けるキーは`id`です。`serialNumber + ts`だけでは、
時計補正や同秒の手動測定で衝突するため、端末側の永続counter等も組み合わせます。

## 時刻

JSONの`ts`はUTC offsetまたは`Z`付きISO 8601にします。NTP同期前の時刻を正しい
日時として偽装しません。サンプルロガーは未同期時に`ts`を空にし、
`error=time_not_synchronized`と安定した`id`を保存します。付属GASはこの場合だけ
server受信時刻をSheetのDateに使用するため、これは測定時刻ではありません。

時系列精度が必要な製品では、RTC保持、同期品質flag、起動時刻とmonotonic offset、
再同期による時刻jumpへの方針を追加してください。

## ネットワークと認証

- URLは`https://`だけを許可し、TLS証明書とhostnameを検証する。
- GASのredirectを最終responseまで安全に追従する。
- HTTP statusだけでなくJSON schemaと`ok:true`を検証する。
- Wi-Fi passwordとtokenをソース、USB log、画面、crash dumpへ出さない。
- timeout、回数制限付きretry、backoff、queue上限を設ける。
- token漏えい時に交換できる手順を用意する。

共有tokenは評価・小規模用途向けです。多数端末ではdevice別credential、失効、
rate limit、監査logを持つ受信APIを検討してください。

## 省電力と電源

sleep／センサ電源制御はソフトウェアだけの機能ではありません。Core2がsleepして
センサが動作し続ける、または逆の場合にUARTから逆給電しない回路が必要です。
EnableをGPIO制御する変更では、端子定格、起動時pin状態、driver、安定待ち、
例外時のOFF順序を評価します。SD writeやHTTPS送信中に電源を落としません。

## 移植時の検証項目

- [配線](m5stack-wiring.ja.md): GPIO、信号方向、電源、片側電源断
- UART: 既知フレーム、分割応答、残留byte、timeout、CRC error
- Protocol: error frame、echo不一致、最大size、signed温度、firmware境界
- Scheduler: 手動／周期の競合、`millis()` wrap、Wi-Fi遅延中の測定
- Storage: 初回header、CSV quoting、満杯、抜去、破損、書込み途中のreset
- Upload: offline復帰、redirect、TLS失敗、401相当、GAS `ok:false`、重複再送
- Time: NTPなし、timezone、再同期、deep sleep復帰
- Endurance: 実設置で連続運転、RSSI、温度、電源余裕、乾湿変化

hardware-in-the-loop試験を行うときも、活線挿抜は行わず、異常挿入は承認済みの
switch／治具で実施してください。
