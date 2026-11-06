# Googleスプレッドシートへの保存

[English](gas-setup.md) | 日本語

`M5Core2CloudLogger`は、測定値をmicroSDへ残したうえで、HTTPS経由でGoogle Apps
Script（GAS）へ送信できます。GASは受信したJSONをGoogleスプレッドシートへ
1行ずつ展開します。Wi-FiやGASが一時的に使えない場合、ロガーはmicroSDの
キューを保持し、後で同じ行IDを使って再送します。
受信後はヘッダ以外の行全体を`Date`の昇順（古い日時→新しい日時）で並べ替えます。
過去の再送行も既存行と合わせて整列し、エラー行も残します。
`setupSheet`を実行しても既存行を整列できます。時刻未同期で`ts`が空の場合、
`Date`は受信時刻なので、元の測定時刻順を復元することはできません。

この構成ではGAS Webアプリを「全員」が呼べるようにし、本文の共有トークンで
書込みを認証します。URLだけでは書き込めませんが、URLとトークンの両方を
入手した人はデータを送信できます。トークンをGitHub、画面写真、サポート用ログ、
チャットへ掲載しないでください。

## 1. 用意するもの

- Googleアカウント
- 測定値を書き込む新しいGoogleスプレッドシート
- このリポジトリの
  [`extras/google-apps-script/Code.gs`](../extras/google-apps-script/Code.gs)
- パスワードマネージャ等で生成した、推測困難なランダムトークン（32 byte以上を推奨）

このサンプルに認証情報や稼働中のWebアプリURLは含まれていません。

## 2. Apps Scriptプロジェクトを作る

1. 測定値を書き込むGoogleスプレッドシートを開きます。
2. 上部メニューの「拡張機能」→「Apps Script」を選びます。
3. 既定の`Code.gs`の内容をすべて削除します。
4. リポジトリの`extras/google-apps-script/Code.gs`を全文コピーして貼り付けます。
5. プロジェクト名を`SLT5006 receiver`等に変更し、保存します。

リポジトリの`appsscript.json`は、`clasp`等でソース管理する方のための参考
manifestです。ブラウザで設定するだけなら貼り付ける必要はありません。

## 3. スクリプトプロパティを設定する

`INGEST_TOKEN`は推測できないランダムな値を使います。Windows PowerShellでは、次の
コマンドで32 byte（16進数64文字）のトークンを生成できます。

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

表示された値をパスワードマネージャ等にも安全に保存し、GASの`INGEST_TOKEN`と
Core2設定の`gas_token`の両方に同じ値を設定してください。

GASエディタ左側の歯車「プロジェクトの設定」を開き、「スクリプト
プロパティを追加」から次を設定します。

| キー | 必須 | 値 |
|---|---|---|
| `INGEST_TOKEN` | 必須 | Core2と共有するランダムトークン |
| `TARGET_SHEET` | 任意 | 書込み先のシート名。省略時は`soil_sensor` |
| `TIME_ZONE` | 任意 | 日時を表示するIANA zone。省略時は`Asia/Tokyo` |
| `SPREADSHEET_ID` | 任意 | standalone scriptから使う場合のsheet ID。bound scriptでは省略 |

`INGEST_TOKEN`はコードへ書かず、スクリプトプロパティへ保存します。前後の空白を
加えないでください。GAS側とCore2設定の`gas_token`は大文字・小文字を含めて
完全一致が必要です。

## 4. ヘッダを準備して権限を承認する

1. GASエディタ上部の関数一覧から`setupSheet`を選びます。
2. 「実行」を押します。
3. 初回だけGoogleアカウントを選び、スプレッドシートへのアクセスを許可します。
4. スプレッドシートに`soil_sensor`（または`TARGET_SHEET`）が作成され、1行目に
   ヘッダが入ったことを確認します。

既存のシートを指定する場合、1行目の既存列名が期待値と異なるとGASは安全のため
追記を拒否します。別名の空シートを使うか、データをバックアップして列構成を
合わせてください。既存データを削除・上書きする処理はありません。

列順は次のとおりです（JSONのキー順には依存しません）。

```text
Date, SerialNumber, Firmware, Battery(V), Temperature(degC), VWC(%), VWC rock(%), VWC coco(%), EC bulk(dS/m), EC pore(dS/m), EC pore coco(dS/m), Id, Core2 Battery(V), WiFi RSSI(dBm), Error
```

列構成を変更して試験用シートを作り直す場合は、Core2を停止し、受信タブ
（既定`soil_sensor`）を削除してから、更新したコードで`setupSheet`を実行します。
スプレッドシート全体や紐づくApps Scriptプロジェクトを削除する必要はありません。
最後の1タブで削除できない場合は、一時的に空の別タブを追加します。
既存データは失われます。コードの更新後は、既存デプロイを新バージョンへ更新してから
Core2を再開してください。SDキューに残る過去の行は再送されます。

## 5. Webアプリとしてデプロイする

1. 右上「デプロイ」→「新しいデプロイ」を選びます。
2. 歯車「種類の選択」→「ウェブアプリ」を選びます。
3. 「次のユーザーとして実行」は**自分**を選びます。
4. 「アクセスできるユーザー」は、匿名のCore2からPOSTできる設定（通常は
   **全員**）を選びます。組織アカウントで選べない場合は管理者ポリシーを確認します。
5. 「デプロイ」を押して必要な権限を承認します。
6. 表示された`https://script.google.com/macros/s/.../exec` URLを控えます。

コードを更新したときは「デプロイ」→「デプロイを管理」→既存デプロイの編集→
「新バージョン」で更新します。新しいデプロイを別に作るとURLが変わるため、Core2の
設定も変更が必要になります。

ブラウザで`/exec` URLを開くと、トークンを使わないhealth responseが表示されます。

```json
{"ok":true,"service":"slt5006-google-sheets-receiver","schemaVersion":2}
```

これはWebアプリへ到達できることだけを確認します。シートへの書込み確認ではありません。

## 6. Core2の設定を作る

USB接続し、シリアルモニタを閉じてPCで次を実行します。
Wi-Fi・クラウド設定はCore2の内蔵NVSへ保存します。NVS未設定時はmicroSDの
`/slt5006.cfg`でも設定できます。microSDには測定ログ・送信待ちデータも保存します。

```powershell
py -m pip install pyserial
py .\scripts\configure_logger.py --port COM4
```

`gas_token`は手順3の`INGEST_TOKEN`と同じ値です。`gas_url`は必ずHTTPSの`/exec`
URLにします。

対話入力で設定し、ツールが待機している間に必要ならCore2をリセットします。
保存成功後は自動再起動します。詳しいUSB手順、JSON入力、範囲、offline動作は
[ロガー設定](logger-setup.ja.md)を参照してください。秘密情報は公開リポジトリへ追加しません。

## 7. 送信を確認する

1. microSDを安全に取り出し、電源OFFのCore2へ装着します。
2. [配線ガイド](m5stack-wiring.ja.md)どおりSLT5006を接続して起動します。
3. 画面でWi-Fi接続を、USB SerialまたはCSVの`ts`でNTP同期結果を確認します。
4. ボタンAの`Manual measurement`で測定するか、設定した周期まで待ちます。
5. ボタンCの`Upload`でキュー送信を要求します。
6. スプレッドシートへ行が追加されることを確認します。必要ならカード上の
   `/slt5006_queue`ファイル数が減ることも確認します。

同じキューを通信タイムアウト後に再送しても、各測定の安定した`id`により同じ行は
再追記されません。GASの応答はHTTP 2xxだけで成功と判断せず、本文の`ok:true`を
ロガーが確認します。

### PCから任意で受信側だけをテストする

PowerShellではトークンをコマンド履歴へ直接書かず、対話入力できます。

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

成功時は`ok=true`、`written=1`です。同じ`$body`を再送すると`written=0`、
`duplicates=1`になれば重複防止も確認できています。確認後、`TEST001`のテスト行を
スプレッドシートから手動で削除してください。

## JSON契約

通常は複数行対応の形式を使います。
以下のシリアル番号と行IDは架空の例です。

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
| `token` | 必須 | `INGEST_TOKEN`と一致する共有トークン |
| `rows` | 通常必須 | 1～100行の配列。互換用に単一行形式も受付 |
| `id` | 推奨 | 再送しても変わらない行ID。省略時はserial + tsからGASが生成 |
| `ts` | 通常必須 | `Z`またはUTC offset付きISO 8601日時。空ならserver受信時刻を使い、`id`必須 |
| `serialNumber` | 必須 | SLT5006のシリアル番号（文字列でも数値でも可） |
| `firmware` | 任意 | `major.minor.revision` |
| `battery_v` | 任意 | SLT5006が報告するセンサ電源電圧 [V] |
| `core2_battery_v` | 任意 | Core2内蔵電池電圧 [V] |
| `temperature_c` | 任意 | 温度 [degC] |
| `vwc_pct` / `vwc_coco_pct` / `vwc_rock_pct` | 任意 | 培地別VWC [%] |
| `ec_bulk_dsm` / `ec_pore_dsm` | 任意 | EC [dS/m] |
| `ec_pore_coco_dsm` | 任意 | 対応firmwareだけのココピート向けPore EC [dS/m] |
| `rssi_dbm` | 任意 | Wi-Fi RSSI [dBm] |
| `error` | 任意 | その測定に対応する短いエラー情報 |

利用不能な値はプロパティを省略します。特に、非対応firmwareの
`ec_pore_coco_dsm`を0にしないでください。GASは欠損値を空欄にします。
日時は閏年を含めて実在する日付か検証し、Python／.NETの小数秒も受け付けます。
自動生成IDはUTCのmillisecond精度の日時を使うため、その精度で衝突し得る測定には
一意な`id`を明示してください。数値または数値文字列を受け付け、省略・null・空文字列は
空欄にします。空白だけの数値文字列は0へ変換せずエラーにします。

GASは1要求を全て検証してから、script lock内で既存ID確認と一括追記を行います。
`rows`を指定する場合は配列が必須で、省略時のみ互換用の単一行形式を受け付けます。
識別子は文字列または有限数値で指定し、真偽値・オブジェクトは受け付けません。
1行でも不正ならその要求は書き込みません。応答例:

```json
{"ok":true,"received":10,"written":8,"duplicates":2}
```

```json
{"ok":false,"error":"invalid_timestamp","message":"rows[0].ts must be ..."}
```

## セキュリティと運用

- PCの非公開設定JSON、トークン、稼働URLをGitへcommitしません。
- NVSはこのサンプルでは暗号化していません。Core2への物理アクセスを制限します。
  SD設定を使う場合はカード上の認証情報も暗号化されないため、カードを安全に保管します。
- トークン漏えい時は、GASの`INGEST_TOKEN`と全Core2の`gas_token`を両方交換します。
- Wi-FiとGASはTLS証明書検証を有効にします。証明書検証を無効化する改造は避けます。
- WebアプリURLへのPOSTにはリダイレクトがあるため、独自クライアントへ移植する場合は
  最終レスポンスまで安全に追従し、JSONの`ok`を確認します。
- Spreadsheetの編集共有先、Googleアカウントの多要素認証、Apps Scriptの実行履歴を
  定期的に確認します。
- この共有トークン方式は小規模な評価用です。利用者別の権限、失効、監査、機密性の
  高いデータが必要なら、認証付きAPI基盤へ移行してください。

## トラブルシューティング

| GAS / Core2の表示 | 確認すること |
|---|---|
| `unauthorized` | `INGEST_TOKEN`と`gas_token`の完全一致、前後空白、使用中のNVS／SD設定 |
| `server_not_configured` | GASのスクリプトプロパティに`INGEST_TOKEN`があるか |
| `invalid_timestamp` | NTP／時計、UTC offsetまたは`Z`を含むISO 8601か |
| `invalid_identifier` | 生成IDに空白、日本語、禁止記号がないか |
| `Header mismatch` | 対象シート1行目。空の別シート名を`TARGET_SHEET`へ設定 |
| `busy` | 同時送信が集中。キューを保ったまま間隔を置いて再送 |
| HTTP成功だがqueueが減らない | 最終JSONが`ok:true`か、GASを新versionへ再デプロイしたか |
| health responseは見えるが追記されない | token、JSON、GAS実行ログ、書込み対象sheetを確認 |

GASのエラー応答は環境上HTTP 200になる場合があるため、ステータスコードだけでなく
必ずJSON本文を確認します。診断時もtokenやWi-Fi passwordをログへ貼り付けないでください。
