# M5Core2CloudLogger設定ガイド

[English](logger-setup.md) | 日本語

`M5Core2CloudLogger`は、SLT5006の測定・画面表示に加えて、microSDへのCSV保存、
Wi-Fi/NTP、GoogleスプレッドシートへのHTTPS送信、失敗時の再送キューを提供します。
最初に[配線ガイド](m5stack-wiring.ja.md)を読み、まず`M5Core2QuickStart`でセンサ情報と
測定値を確認してからロガーへ進むことを推奨します。

## microSDを準備する

Core2の電源を切ってmicroSDを装着します。認識できるファイルシステムと容量は
使用するCore2／ライブラリの資料を確認してください。カード上の必要なデータを
バックアップしてから使用します。

microSDには測定CSVと送信待ちキューを保存します。

## 設定の読み込み優先順位

1. NVSに保存済みの設定があれば、その設定を使用します。
2. NVSが未設定の場合だけ、microSDルートの`/slt5006.cfg`を読み込みます。
3. どちらもなければ、60秒周期・offlineの既定値で動作します。

設定は項目ごとに混合しません。NVSに保存したoffline設定（USBから`{}`を送った場合も
含む）があれば、SDの設定は読みません。NVSの読み出しエラーや破損は未設定とは扱わず、
画面・USB Serialにエラーを表示してoffline既定値で動作します。
SDから読み込んだ設定をNVSへ自動保存することはありません。

## microSDから設定する

NVS未設定で使う場合は、microSDルートにUTF-8の`/slt5006.cfg`を用意します。
カードへの設定ファイルの書き込みの代わりに、USB経由でNVSへ設定を保存することもできます。
手順は次節を参照してください。

```ini
wifi_ssid=YOUR_WIFI_SSID
wifi_password=YOUR_WIFI_PASSWORD
gas_url=https://script.google.com/macros/s/REPLACE_WITH_DEPLOYMENT_ID/exec
gas_token=REPLACE_WITH_RANDOM_TOKEN
sample_interval_sec=60
upload_batch_size=10
ntp_server=pool.ntp.org
timezone=JST-9
deep_sleep_sec=0
```

編集後はCore2を再起動します。最大4096 bytesで、LF／CRLFの改行を使用できます。
空行、先頭が`#`／`;`の行は無視します。キー・値の外側の空白は除去し、
値全体を一重引用符または二重引用符で囲めば内側の空白を保持します。
同じキーが複数ある場合は最後の値を使用します。
未知のキー、`=`のない行、不正な整数、範囲外の値、NUL文字はエラーです。
文字列の上限とクラウド設定の条件はUSB設定と共通です。
検証に失敗したファイルは一部だけ適用せず、エラーを表示してoffline既定値で動作します。
カード上の認証情報は暗号化されないため、カードの物理アクセスも制限してください。

## USBからNVSへ設定する

Wi-Fi・クラウド設定と測定間隔等は、Core2内蔵フラッシュのNVSへ保存します。
NVSはアプリ領域とは別のパーティションで、通常のプログラムUploadと電源OFFで
保持されます。全フラッシュ消去、NVS消去、パーティション変更では保持を保証しません。
保存は暗号化していないため、Core2への物理アクセスを制限してください。
USB転送、NVSへの保存、Core2からmicroSDへのログ書込みが社内ポリシーで許可されて
いることを確認してから使用します。

1. ロガーfirmwareをCore2へ書き込み、USB接続します。
2. VS Code／Arduinoのシリアルモニタを閉じます。
3. PCで次を実行します（COM番号は実機に合わせます）。

```powershell
py -m pip install pyserial
py .\scripts\configure_logger.py --port COM4
```

Wi-Fi SSID、password、GAS URL、token、その他の設定を対話入力します。
password・URL・tokenは非表示入力です。既定値を使う項目はEnterを押します。
設定はPC上のファイルへ保存せず、USBへ送ります。入力後にツールが待機状態になったら、
Core2がsleep中または応答しない場合はCore2のリセットボタンを押してください。
起動時の3秒間はセンサ・Wi-Fi処理より先にUSB設定を受け付けます。

`NVS configuration saved`と表示されると保存成功です。Core2は自動再起動し、
NVSの設定で動作します。SDなしでも設定できます。再ビルドは不要です。
エラーや確認応答のタイムアウトは成功ではありません。ポート、入力値、実機を確認します。

必要なら、PCのローカルディスクに置いた非公開JSONを送れます。

```powershell
py .\scripts\configure_logger.py --port COM4 --file .\slt5006-config.json
```

```json
{
  "wifi_ssid": "YOUR_WIFI_SSID",
  "wifi_password": "YOUR_WIFI_PASSWORD",
  "gas_url": "https://script.google.com/macros/s/REPLACE_WITH_DEPLOYMENT_ID/exec",
  "gas_token": "REPLACE_WITH_RANDOM_TOKEN",
  "sample_interval_sec": 60,
  "upload_batch_size": 10,
  "ntp_server": "pool.ntp.org",
  "timezone": "JST-9",
  "deep_sleep_sec": 0
}
```

設定ファイル、Wi-Fi password、GAS token、稼働URLをGitHubへcommitしないでください。
`slt5006-config.json`はgitignore対象ですが、別名で保存したファイルは対象ではありません。

### USBプロトコル（115200 baud）

`CONFIG BEGIN`と改行を送り、`[CONFIG] READY`を待ってから、UTF-8 JSONオブジェクト
1行と改行を送ります。最大2048 bytes（末尾改行を除く）、LF／CRLF対応です。
検証・NVS保存に成功した場合のみ`[CONFIG] SAVED`を返し、再起動します。
失敗は`[CONFIG] ERROR ...`です。値や認証情報を応答へ出力しません。
`CONFIG CANCEL`で中止できます。設定モードは測定・送信・sleep移行を一時停止し、
120秒で解除されます。途中で途切れた行は10秒で破棄します。
検証に失敗した入力は保存せず、中止コマンドも新たな書込みを行いません。
保存応答が失われた場合は保存済みの可能性があるため、実機を確認してください。
設定モード中の測定は中止後にtimeoutとなる
場合があるため、通常はツール待機中にリセットして設定してください。

## 設定キー

| キー | 既定値／範囲 | 説明 |
|---|---|---|
| `wifi_ssid` | 空 | 接続先SSID。空ならWi-Fi無効 |
| `wifi_password` | 空 | Wi-Fi password |
| `gas_url` | 空 | HTTPSのGAS `/exec` URL。空ならupload無効 |
| `gas_token` | 空 | GASの`INGEST_TOKEN`と同じ値 |
| `sample_interval_sec` | 60、5～86400 | 連続動作時の測定開始間隔 [秒] |
| `upload_batch_size` | 10、1～20 | 1回のPOSTで送る最大行数 |
| `ntp_server` | `pool.ntp.org` | NTP server名 |
| `timezone` | `JST-9` | ESP32 POSIX timezone文字列 |
| `deep_sleep_sec` | 0、または10～86400 | 0=連続動作。10以上=測定後のdeep sleep [秒] |

`timezone`はIANA名ではなくESP32/POSIX形式です。日本は`JST-9`です。別地域では
UTC offsetの符号が一般的な表記と逆になる点や夏時間規則に注意してください。
ロガーのJSON/CSV `ts`は地域に依存しないUTCの`Z`形式です。GAS側の`TIME_ZONE`は
Sheet表示用のIANA名（例:`Asia/Tokyo`）で、別の形式です。

USB設定は毎回、設定全体を置き換えます。省略したキーは既定値に戻ります。`{}`を送れば、
このロガーの設定をoffline既定値へ戻せます（他のNVSデータは消去しません）。
未知のキー、不正な型、範囲外の整数はエラーで保存しません。数値はJSONの整数で指定し、
文字列・小数・真偽値では指定しません。文字列の空白はpasswordも含めそのまま保存します。
文字列の上限はUTF-8 bytesでSSID 32、password 64、URL 512、token 256、
NTP server 253、timezone 128です。NUL文字、空のNTP server／timezoneは使用できません。
クラウドを使う場合はSSID・HTTPS URL・tokenを全て指定し、無効にする場合は
URL・tokenを両方空にします。PCツールは重複キーもエラーにします。

## microSDがない場合

microSDはセンサの測定と画面表示には必須ではありません。カードがない、または
初期化に失敗した場合も、Core2はNVSの設定周期で測定し、画面とUSB Serialに
結果を表示します。Wi-Fi接続・NTPもNVS設定に従って動作します。ただし、
次の機能は利用できません。

- `/slt5006.csv`への記録
- `/slt5006_queue`への未送信行保存と再送
- クラウドへの送信（SDキューを使用するため、直接送信はしません）

SDがなく、NVSも未設定なら、60秒周期・offlineで測定します。

画面のSD errorを解消してから、記録・クラウド運用を開始してください。

## 操作

画面は項目名・右寄せの数値・単位の列を固定して表示します。
全培地で温度（`Temp`）、VWC、EC bulkの順に表示します。
電池欄の下にセンサのシリアルNo.とFirmwareバージョンを表示します。
センサ情報を取得できていない場合は`unavailable`を表示します。
センサ測定に失敗した後は、保持した直近の正常値に`(STALE)`を付けます。
uploadが成功しても表示を残し、次の測定成功で解除します。
測定周期（既定60秒）と画面更新周期（0.5秒）は別です。画面はバッファ上で描画して
まとめて転送し、更新時の黒画面によるちらつきを抑えます。
測定時間が`sample_interval_sec`を超えても、完了後に送信用のidle loopを確保します。
長い測定やtimeoutが続いても、次の測定開始で送信・再接続を妨げません。
バッファ確保に失敗した場合はエラーを表示し、直接描画で動作します。
`SLT5006 supply`はセンサが報告する供給電圧です。背景色を分けた
`Core2 battery (est.)`欄はCore2の残量の目安と電圧で、センサ測定成功前も表示します。
メインループが動作している間は5秒ごとに取得し、残量%は負荷や充電状態で変動する目安です。
取得できない場合は`unavailable`と表示し、シリアルモニタにも出力します。
CSV・クラウドの記録形式は変更せず、残量%は画面表示のみです。

画面下部のタッチ領域を使います。

| ボタン | 動作 |
|---|---|
| A | `Manual measurement`: 周期を待たず測定を要求。測定中はその完了を待つ |
| B | 表示する培地を`General` → `Rock wool` → `Coco`の順に切替 |
| C | `Upload/reconnect`: Wi-Fi再接続と保存済みキューの送信を要求 |

ボタンBは表示項目の選択です。センサが異なる独立した3種類の水分を同時測定する
わけではありません。一般土壌は`vwc_pct`、ロックウールは`vwc_rock_pct`、
ココピートは`vwc_coco_pct`を使用します。Pore ECも用途と製品資料の適用条件を
確認してください。
ロックウール選択時は`EC pore`行を表示しません。
ココピート選択時、firmwareが非対応または不明ならPore ECを`--`にします。
一般土壌のPore ECで代用しません。

この切り替えは画面表示だけに適用され、CSV保存・クラウド送信の対象は変わりません。
選択中の培地に関係なく3種類のVWC換算値とEC値を記録し、ココピート向けPore ECは
センサfirmwareが対応する場合のみ送信します。Core2残量%は送信せず、電池電圧は従来どおり記録します。
送信先は使用中のNVSまたはSD設定の`gas_url`（HTTPSの`/exec` URL）です。
`gas_token`はGASの`INGEST_TOKEN`と一致させます。USB保存成功後は自動再起動し、
SD設定の編集後は手動で再起動します。
設定変更のための再ビルドは不要です。

測定変換待ちは協調ポーリングのため、画面とネットワーク処理を続けられます。
ただし1回のUART取引、microSD書込み、HTTPS通信は短時間停止することがあります。

## 保存ファイルと再送

成功した測定とセンサ通信エラーは`/slt5006.csv`へappendされます。エラー行は測定値が空欄で、`error`列に理由が入ります。列名に単位を含む名前付き形式で、
代表的な列は次のとおりです。

```text
id,ts,serialNumber,firmware,battery_v,core2_battery_v,temperature_c,vwc_pct,vwc_coco_pct,vwc_rock_pct,ec_bulk_dsm,ec_pore_dsm,ec_pore_coco_dsm,rssi_dbm,error
```

CSVをExcel等で開いたままカードへ書き戻さず、Core2を停止してカードを安全に
取り外してからコピーしてください。`battery_v`はセンサが報告するセンサ電源、
`core2_battery_v`はCore2内蔵電池です。
Core2電池電圧を取得できない場合、JSONはnull、CSVは空欄にし、0や負値の
測定値にはしません。RSSIの`-127`はWi-Fi未接続を示します。
CSVはヘッダ・行の全バイトが書き込まれたことを確認します。部分書込みは失敗として
表示し、queueへの保存が成功した行はクラウド配送用に保持します。SD書込みエラー後は
Core2を停止してCSVを確認・バックアップしてから再開してください。
ファイルに不完全な行が残る場合があります。

クラウド送信待ちの各行は`/slt5006_queue`内へ安定した`id`とともに保存されます。
送信前にキュー全体を走査し、JSONのUTC測定日時`ts`が古い行から選びます。
`ts`が空の時刻未同期行は日時ありの行より先に送り、同じ日時（空も含む）は
ファイル名順で選びます。既定10行、最大20行を保持して送るため、
メモリ使用量はキュー全件数には比例しませんが、走査時間は件数に応じて増えます。
キューが更新されなければ、成功したバッチを削除するたび古い順に消化します。
CSVの並びと保存形式、成功応答を確認してからキューを削除する条件は変わりません。
各JSONファイルは最大2048 bytesで、既存IDを上書きしません。サイズ超過、読出し不足、
NUL、不正JSON（末尾の余分なdataを含む）はファイルを残して送信batchから除外し、
USB Serialへファイル名を表示します。キューを開けない場合や、不正な行だけが
残る場合は、空キューの成功として扱わずエラーを表示します。
送信はHTTPS 2xxに加えてGAS JSONの`ok:true`を確認したときだけ成功です。失敗・
タイムアウト・応答不明ならファイルを残し、次回またはボタンCで同じIDのまま
再送します。GASも保存済みIDを検出するため、応答だけ失われた場合の二重行を防ぎます。
GASの結果取得先への302／303リダイレクトは、新しい接続のGETで取得します。
USB Serialの`[UPLOAD] hop=... method=... status=...`で、最初のPOSTと
結果取得GETのどちらが失敗したか確認できます。HTTPS以外への転送は行いません。
各要求は接続を再利用せず、HTTP終了処理の前にTLS接続を明示的に閉じます。
これにより、リダイレクト本文を破棄するときの不要な受信確認を避けます。
POST・GET・応答本文の読み取り中のエラーを隠す変更ではありません。
ブロッキング処理の前に画面を再描画し、`Connecting Wi-Fi...`、
`Preparing upload...`、`Uploading ... row(s)...`、
`Fetching upload result...`を`Measuring...`と区別して表示します。
HTTPS送信自体は同期処理のため、その間はボタンを処理しません。
`[UPLOAD] accepted_rows=... queue_cleaned=1`は、成功応答の確認と
SDキューからの対象バッチ削除が両方完了したことを示します。

過去のセンサ通信失敗も再送対象です。`serialNumber=0`、firmwareや測定値が空欄で、
`error`に`sensor response timed out`がある行は、センサ情報を取得できなかった
試行のエラー記録です。現在の正常測定とは`id`と日時で区別してください。

キューは運用中のデータです。手動削除やID変更をしないでください。microSD故障や
容量不足に備え、CSVを定期的に別媒体へバックアップし、カード上のqueueファイル数と
空き容量を監視します。

## Deep sleep

`deep_sleep_sec=0`は通常の連続動作です。10以上を指定すると省電力モードになり、
起床直後に1回の測定を試み、SD保存、upload試行後に指定秒数sleepします。センサ
失敗時もその試行後にsleepします。sleep中は画面、Wi-Fi、ボタン操作、測定を行い
ません。このモードでは`sample_interval_sec`を使用しません。再起動、センサ安定待ち、
Wi-Fi/NTP接続が加わるため、実際のstart-to-start間隔はsleep秒数より長くなります。

電池運用へ組み込む前に、次を実機評価してください。

- Core2、Bottom、センサ電源がsleep中にどうなるか
- UARTからの逆給電が起きないこと
- NTP失敗時の時刻、Wi-Fi接続時間、消費電流
- SD書込み完了後にsleepすること、電源断からのファイル回復
- `sample_interval_sec`との使い分けと、実際のstart-to-start周期

## 期待する確認順序

1. `M5Core2QuickStart`でFirmware、Serial number、温度、VWC、ECを表示する。
2. NVSとSD設定がない状態で起動し、offline動作とUSB設定案内を確認する。
3. USBから設定し、USB SerialまたはCSVのUTC `ts`と1行を確認する。
4. Wi-Fiを一時的に切り、測定がCSVとqueueへ残ることを確認する。
5. Wi-Fiを戻してボタンCを押し、GASへ1回だけ追記されqueueが減ることを確認する。
6. センサを電源OFFで外し、timeout表示と、その後の復旧を確認する。
7. 実際の設置場所でRSSI、連続運転、電源、SD容量、乾湿変化を評価する。
8. 電源OFFと通常のfirmware Upload後も設定が残ること、SDなしの設定、
   不正入力・中止、deep sleep中のリセットからの設定を確認する。
9. NVS未設定でSD設定が適用されること、NVS保存後はSD設定より優先されることを確認する。
10. ココピートECの非対応・不明時の`--`、センサ失敗後の`(STALE)`、
    SD失敗時も送信待ち行が保持されることを確認する。
11. 5秒周期と承認済みの低速センサ治具で、長い測定やtimeoutの間にも
    送信・再接続が実行されることを確認する。

## よくある問題

| 状態 | 最初に確認すること |
|---|---|
| Sensor timeout | [配線](m5stack-wiring.ja.md)、選択したGPIO（PORT.A 32/33またはPORT.C 13/14）、電源、Enable、TX/RX、9600 8N1 |
| SD error | カード装着、対応format、接点、空き容量、電源OFFでの再装着 |
| Wi-Fi offline | SSID/password、2.4 GHz AP、電波、使用中のNVS／SD設定 |
| 時刻が不正 | Internet/NTP、`ntp_server`、USB Serial/CSVの`ts`と`time_not_synchronized` |
| Queueが減らない | `gas_url`、token、GAS deployment、JSON `ok`、実行ログ |
| Upload transport -1 | Wi-Fi接続後のDNS／TCP／TLS接続失敗。USB Serialの`[UPLOAD]`診断を確認 |
| Coco Pore ECが空 | firmware 1.7.5未満またはfirmware情報を取得できていない（正常な省略） |

クラウド設定は[GASセットアップ](gas-setup.ja.md)、自社システムへの移植は
[組み込みガイド](m5stack-integration.ja.md)を参照してください。

Wi-Fiの`online`はアクセスポイントへの接続状態であり、GASへの到達を保証しません。
負の送信結果はサーバが返したHTTPステータスではなく、ESP32通信ライブラリのエラーです。
原因を確認するには115200 baudのシリアルモニタを開き、ボタンCで再送を要求します。
`[UPLOAD]`行には通信エラーの説明、TLSエラーコードと説明、Wi-Fi状態、RSSI、
時刻の有効性を出力します。URL・token・送信内容は出力しません。
`clock_valid=0`なら時刻同期を確認し、TLSエラーなら証明書検証・時刻・通信経路を
確認してください。TLSエラーが0でもDNS／TCP接続等が失敗している可能性があります。
証明書検証を無効化せず、診断情報から原因を特定します。失敗したデータはSDキューに残ります。
Google向けの信頼証明書はGTS Root R1／R4を組み込んでいます。
TLS `-9984`は証明書検証の失敗です。信頼ルート不足のほか、実時刻のずれや
通信経路での証明書置換等でも発生します。`clock_valid=1`は2024年以降であることの
確認だけなので、USBの測定JSONにあるUTC `ts`も実際の時刻と比較してください。
