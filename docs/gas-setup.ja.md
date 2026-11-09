# Googleスプレッドシートへの保存

[English](gas-setup.md) | 日本語

`M5Core2CloudLogger`は、測定値をmicroSDに保存したうえで、HTTPSでGoogle Apps Script（GAS）へ送信できます。GASは受信したJSONを、1測定につき1行としてGoogleスプレッドシートに書き込みます。Wi-FiやGASが一時的に使えないときは、ロガーが送信待ちのデータをmicroSDのキューに残し、後で同じ行IDのまま再送します。

受信後、GASはヘッダ行を除くすべての行を`Date`の昇順（古い日時から新しい日時の順）に並べ替えます。再送された過去の行も既存の行と合わせて並べ替え、エラー行も削除せずに残します。既存の行は、`setupSheet`を実行して並べ替えることもできます。時刻が未同期のまま測定して`ts`が空になった行は、`Date`が受信時刻になるため、元の測定順には並べ替えられません。

この構成では、GASのWebアプリを「全員」がアクセスできる設定でデプロイし、送信するJSONに含めた共有トークンで書き込みを認証します。URLだけでは書き込めませんが、URLとトークンの両方を知っている人はデータを送信できます。トークンは、GitHub、画面写真、サポート用のログ、チャットに載せないでください。

## 1. 用意するもの

- Googleアカウント
- 測定値を書き込む新しいGoogleスプレッドシート
- このリポジトリの[`extras/google-apps-script/Code.gs`](../extras/google-apps-script/Code.gs)
- パスワードマネージャーなどで生成した、推測できないランダムなトークン（32バイト以上を推奨）

このサンプルには、認証情報や稼働中のWebアプリのURLは含まれていません。

## 2. Apps Scriptプロジェクトを作る

1. 測定値を書き込むGoogleスプレッドシートを開きます。
2. 上部メニューの「拡張機能」→「Apps Script」を選びます。
3. 既定の`Code.gs`の内容をすべて削除します。
4. リポジトリの`extras/google-apps-script/Code.gs`の内容をすべてコピーして貼り付けます。
5. プロジェクト名を`SLT5006 receiver`などに変更して保存します。

リポジトリの`appsscript.json`は、`clasp`などでソースを管理する場合に使う参考用のマニフェストです。ブラウザだけで設定する場合は、貼り付ける必要はありません。

## 3. スクリプトプロパティを設定する

`INGEST_TOKEN`には、推測できないランダムな値を使います。Windows PowerShellでは、次のコマンドで32バイト（16進数64文字）のトークンを生成できます。

```powershell
$bytes = New-Object byte[] 32
$rng = [System.Security.Cryptography.RandomNumberGenerator]::Create()
try {
  $rng.GetBytes($bytes)
  $token = [BitConverter]::ToString($bytes).Replace('-', '').ToLowerInvariant()
  $token
}
finally {
  $rng.Dispose()
}
```

表示された値をパスワードマネージャーなどの安全な場所に保管し、GASの`INGEST_TOKEN`とCore2の設定項目`gas_token`の両方に同じ値を設定してください。

GASエディタ左側の歯車アイコン「プロジェクトの設定」を開き、「スクリプト プロパティを追加」から次のプロパティを設定します。

| キー | 必須 | 値 |
|---|---|---|
| `INGEST_TOKEN` | 必須 | Core2と共有するランダムなトークン |
| `TARGET_SHEET` | 任意 | 書き込み先のシート名。省略時は`soil_sensor` |
| `TIME_ZONE` | 任意 | 日時の表示に使うIANAタイムゾーン名。省略時は`Asia/Tokyo` |
| `SPREADSHEET_ID` | 任意 | スタンドアロンスクリプトで使う場合のスプレッドシートID。手順2のようにスプレッドシートから作成したスクリプトでは省略 |

`INGEST_TOKEN`はコードに書かず、スクリプトプロパティに保存します。値の前後に空白を入れないでください。GAS側の値とCore2の`gas_token`は、大文字・小文字も含めて完全に一致させてください。

## 4. ヘッダを準備して権限を承認する

1. GASエディタ上部の関数一覧から`setupSheet`を選びます。
2. 「実行」を押します。
3. 初回だけ、Googleアカウントを選んでスプレッドシートへのアクセスを許可します。
4. スプレッドシートに`soil_sensor`シート（`TARGET_SHEET`を設定した場合はそのシート）が作成され、1行目にヘッダが入ったことを確認します。

既存のシートを指定した場合、1行目の列名がGASの想定と異なると、GASは安全のため追記しません。別の名前の空のシートを使うか、データをバックアップしてから列構成を合わせてください。GASが既存のデータを削除したり上書きしたりすることはありません。

列の順序は次のとおりです。JSONのキーの順序には依存しません。

```text
Date, SerialNumber, Firmware, Battery(V), Temperature(degC), VWC(%), VWC rock(%), VWC coco(%), EC bulk(dS/m), EC pore(dS/m), EC pore coco(dS/m), Id, Core2 Battery(V), WiFi RSSI(dBm), Error
```

列構成を変更した後に試験用のシートを作り直すときは、Core2を停止し、受信用のタブ（既定は`soil_sensor`）だけを削除してから、更新したコードで`setupSheet`を実行します。スプレッドシート本体と、紐づいたApps Scriptプロジェクトはそのまま残します。受信用のタブしかない場合は削除できないため、先に空のタブを一時的に追加します。削除したタブのデータは失われます。コードを更新したら、既存のデプロイを新しいバージョンに更新してから、Core2を再開してください。SDカードのキューに残っている過去の行は、再開後に再送されます。

## 5. Webアプリとしてデプロイする

1. 右上の「デプロイ」→「新しいデプロイ」を選びます。
2. 「種類の選択」の歯車アイコンから「ウェブアプリ」を選びます。
3. 「次のユーザーとして実行」で**自分**を選びます。
4. 「アクセスできるユーザー」では、Core2が匿名でPOSTできる設定（通常は**全員**）を選びます。組織のアカウントでこの設定を選べない場合は、管理者が設定したポリシーを確認してください。
5. 「デプロイ」を押し、求められた権限を承認します。
6. 表示された`https://script.google.com/macros/s/.../exec`のURLを控えます。

コードを更新したときは、「デプロイ」→「デプロイを管理」で既存のデプロイを編集し、「新バージョン」を選んで更新します。新しいデプロイを別に作るとURLが変わるため、Core2の設定も変更が必要です。

ブラウザで`/exec`のURLを開くと、次のヘルスチェック応答が表示されます。この応答はトークンなしで取得でき、秘密情報を含みません。

```json
{"ok":true,"service":"slt5006-google-sheets-receiver","schemaVersion":2}
```

この応答で確認できるのは、Webアプリに到達できることだけです。トークンの認証やシートへの書き込みは確認できません。

## 6. Core2を設定する

Core2をPCにUSBで接続し、シリアルモニタを閉じてから、PCで次のコマンドを実行します。Wi-Fiとクラウドの設定は、Core2内蔵のNVSに保存されます。NVSが未設定のときは、microSDの`/slt5006.cfg`でも設定できます。microSDには、測定ログと送信待ちのデータも保存されます。

```powershell
py -m pip install pyserial
py .\scripts\configure_logger.py --port COM4
```

`gas_token`には、手順3の`INGEST_TOKEN`と同じ値を入力します。`gas_url`には、必ず手順5で控えたHTTPSの`/exec`のURLを入力します。

ツールの案内に従って、設定値を対話形式で入力します。ツールが待機しているときにCore2がスリープ中または応答しない場合は、Core2をリセットしてください。保存に成功すると、Core2は自動で再起動します。USBでの設定手順の詳細、JSONやmicroSDでの入力、既定値と設定範囲、NVSでの設定の保持、オフライン時の動作は、[ロガー設定](logger-setup.ja.md)を参照してください。

## 7. 送信を確認する

1. microSDカードを安全に取り外し、電源を切った状態のCore2に挿入します。
2. [配線ガイド](m5stack-wiring.ja.md)のとおりにSLT5006を接続し、Core2を起動します。
3. Wi-Fiに接続できたことを画面で確認します。NTPによる時刻同期の結果は、USBシリアルの出力またはCSVの`ts`で確認します。
4. ボタンAの`Manual measurement`で測定するか、設定した測定周期になるまで待ちます。
5. ボタンCの`Upload`で、キューに残っているデータの送信を要求します。
6. スプレッドシートに行が追加されたことを確認します。microSDカードの`/slt5006_queue`内のファイル数が減ったことでも確認できます。

通信がタイムアウトした後に同じキューを再送しても、各測定の`id`は変わらないため、同じ行が重複して追記されることはありません。ロガーはHTTP 2xxだけで成功とは判断せず、応答本文の`ok:true`を確認します。

### PCから受信側だけをテストする（任意）

次のPowerShellの例では、トークンをコマンドに直接書かずに対話入力で受け取るため、トークンがコマンド履歴に残りません。

```powershell
$url = 'https://script.google.com/macros/s/REPLACE_WITH_DEPLOYMENT_ID/exec'
$secureToken = Read-Host 'INGEST_TOKEN' -AsSecureString
$token = [System.Net.NetworkCredential]::new('', $secureToken).Password
$rowId = 'manual-test-' + [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()
$body = @{
  token = $token
  rows = @(@{
    id = $rowId
    ts = [DateTimeOffset]::Now.ToString('o')
    serialNumber = 'TEST001'
    firmware = 'test'
    temperature_c = 20.0
    vwc_pct = 30.0
    ec_bulk_dsm = 0.1
  })
} | ConvertTo-Json -Depth 4
Invoke-RestMethod -Method Post -Uri $url -ContentType 'application/json' -Body $body
```

成功すると`ok=true`、`written=1`が返ります。同じ`$body`をもう一度送って`written=0`、`duplicates=1`が返れば、重複防止も動作しています。確認が終わったら、`TEST001`のテスト行をスプレッドシートから手動で削除してください。

## JSONの仕様

通常は、複数行をまとめて送れる次の形式を使います。シリアル番号と行IDは架空の例です。

```json
{
  "token": "shared-secret",
  "rows": [
    {
      "id": "example-measurement-001",
      "ts": "2026-08-12T10:15:30+09:00",
      "serialNumber": "12345678",
      "firmware": "1.7.6",
      "battery_v": 4.97,
      "core2_battery_v": 4.08,
      "temperature_c": 24.5,
      "vwc_pct": 31.2,
      "vwc_coco_pct": 30.8,
      "vwc_rock_pct": 32.1,
      "ec_bulk_dsm": 0.431,
      "ec_pore_dsm": 1.105,
      "ec_pore_coco_dsm": 1.087,
      "rssi_dbm": -61,
      "error": ""
    }
  ]
}
```

| フィールド | 必須 | 意味 |
|---|---|---|
| `token` | 必須 | `INGEST_TOKEN`と同じ値の共有トークン |
| `rows` | 通常必須 | 1～100行分の配列。互換用の単一行形式も使用可 |
| `id` | 推奨 | 再送しても変わらない行ID。省略時はシリアル番号と`ts`からGASが生成 |
| `ts` | 通常必須 | `Z`またはUTCオフセット付きのISO 8601日時。空の場合はサーバーの受信時刻を使い、`id`が必須 |
| `serialNumber` | 必須 | SLT5006のシリアル番号（文字列・数値のどちらも可） |
| `firmware` | 任意 | `major.minor.revision`形式のファームウェアバージョン |
| `battery_v` | 任意 | SLT5006が報告するセンサの電源電圧 [V] |
| `core2_battery_v` | 任意 | Core2の内蔵バッテリー電圧 [V] |
| `temperature_c` | 任意 | 温度 [degC] |
| `vwc_pct` / `vwc_coco_pct` / `vwc_rock_pct` | 任意 | 培地別のVWC [%] |
| `ec_bulk_dsm` / `ec_pore_dsm` | 任意 | EC [dS/m] |
| `ec_pore_coco_dsm` | 任意 | ココピート向けPore EC [dS/m]。対応ファームウェアのみ |
| `rssi_dbm` | 任意 | Wi-Fi RSSI [dBm] |
| `error` | 任意 | その測定に関する短いエラー情報 |

取得できない値は、プロパティごと省略します。特に、ファームウェアが`ec_pore_coco_dsm`に対応していない場合や、対応しているか判別できない場合は、0を入れずに省略してください。GASは省略された値を空欄にします。

測定値には、数値または数値を表す文字列を指定します。省略した値、null、空文字列は空欄になります。空白だけの文字列は0に変換せず、エラーにします。

日時は、閏年も考慮して実在する日付かどうかを検証します。Pythonや.NETが出力する小数秒付きの日時も受け付けます。`id`を省略すると、GASはシリアル番号とUTCのミリ秒単位の日時からIDを生成します。同じシリアル番号でミリ秒単位まで同じ日時になり得る測定には、一意な`id`を指定してください。

GASは、1回の要求に含まれるすべての行を検証してから、スクリプトロックを取得し、既存のIDを除いた行をまとめて追記します。不正な行が1行でもあれば、その要求の行はどれも書き込みません。`rows`を指定する場合は配列にしてください。互換用の単一行形式は、`rows`を省略したときだけ受け付けます。`serialNumber`と`id`には、文字列または有限の数値を指定してください。真偽値やオブジェクトは受け付けません。応答の例を次に示します。

```json
{"ok":true,"received":10,"written":8,"duplicates":2}
```

```json
{"ok":false,"error":"invalid_timestamp","message":"rows[0].ts must be ..."}
```

## セキュリティと運用

- PC上の非公開の設定JSON、トークン、稼働中のWebアプリのURLは、Gitにコミットしないでください。
- このサンプルではNVSを暗号化していません。Core2に物理的に触れられる人を限定してください。microSDの設定ファイルを使う場合は、カード上の認証情報も暗号化されないため、カードを安全に保管してください。
- トークンが漏れたときは、GASの`INGEST_TOKEN`と、すべてのCore2の`gas_token`を新しい値に交換してください。
- Wi-Fi経由でGASと通信するときのTLS証明書の検証は、有効のままにしてください。証明書の検証を無効にする改造はしないでください。
- WebアプリのURLにPOSTすると、リダイレクトが返ります。独自のクライアントに移植する場合は、リダイレクトを安全にたどって最終的な応答を受け取り、JSONの`ok`を確認してください。
- スプレッドシートの共有先（特に編集権限）を限定し、Googleアカウントの多要素認証を有効にしてください。これらの設定とApps Scriptの実行履歴は、定期的に確認してください。
- この共有トークン方式は、評価や小規模な運用向けです。利用者や機器ごとの権限、認証情報の失効、監査が必要な場合や、機密性の高いデータを扱う場合は、認証機能を持つAPI基盤に移行してください。

## トラブルシューティング

| GAS / Core2の表示 | 確認すること |
|---|---|
| `unauthorized` | `INGEST_TOKEN`と`gas_token`が完全に一致しているか。前後に空白がないか。NVSとSDのどちらの設定が使われているか |
| `server_not_configured` | GASのスクリプトプロパティに`INGEST_TOKEN`が設定されているか |
| `invalid_timestamp` | NTPと時計の状態。日時が`Z`またはUTCオフセット付きのISO 8601形式か |
| `invalid_identifier` | 生成したIDに空白、日本語、使用できない記号が含まれていないか |
| `Header mismatch` | 対象シートの1行目の列名。別の空のシートを使う場合は、そのシート名を`TARGET_SHEET`に設定 |
| `busy` | 送信が同時に集中していないか。キューを残したまま、間隔を置いて再送 |
| HTTPは成功したがキューが減らない | 最終的なJSON応答が`ok:true`か。コードの変更後、GASを新しいバージョンで再デプロイしたか |
| ヘルスチェック応答は表示されるが行が追記されない | トークン、送信したJSON、GASの実行ログ、書き込み先のシート |

Apps Scriptの仕様上、GASのエラー応答もHTTP 200で返ることがあります。ステータスコードだけで判断せず、必ずJSON本文を確認してください。診断のためにログを共有するときも、トークンやWi-Fiパスワードは貼り付けないでください。
