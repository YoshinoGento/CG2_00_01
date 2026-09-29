# 水道農業 実行版 / 2026-09-28

## 最新版

- ZIP: `generated/kurihaku_20260928/SuidoNogyo_Runtime_20260928_02.zip`
- フォルダー: `generated/kurihaku_20260928/suido_nogyo_named/SuidoNogyo`
- 起動: ZIPを新しいフォルダーに展開し、`StartGame.cmd`。本体は `SuidoNogyo.exe`。
- サイズ: 34,513,213 bytes
- SHA256: `3177018d8a5fbf62e3d83dc149ba3edc70b7f6d2fb18000f88d71fef66e25f02`

## 変更

WinApp::Initializeのcaptionを「水道農業」に変更。Unicodeエスケープのwide literalでソースコードページに依存させない。
main.cppのDirectX12起動失敗ダイアログcaptionをSuidoNogyoに統一。例外処理とerror.what()の扱いは変更しない。
ウィンドウクラス識別子CG2WindowClassは内部識別子として維持。
ゲーム進行、UI入力、保存ロジック、所有権、DX12同期/Barrier/Fence/Descriptor/DrawCallは変更なし。
今回の差分に新しいnullptr/OOB/寿命/動的確保/不要コピー経路はない。既存全体の安全性評価ではない。

`build_suido_nogyo.py`で861ファイルを隔離コピーしてRelease/x64をビルド。VS18/v145、0警告・0エラー、1分33.59秒。
`package_suido_nogyo.py`で同じビルドのEXE/DXC DLLとハッシュ照合した素材を集約。
ZIP内部の全パスはASCII。ソース、PDB、LIB、ログ、開発用Settingsは同梱しない。
技術的な配布名と正式な選考会提出ZIPの氏名等の命名指定は別。今回の成果物は実行版のみ。

## 検証

- 配布対象353ファイルと展開後のSHA256一致、ZIP CRC正常。
- ゲーム本体のウィンドウ上部とWindowsの取得タイトルが「水道農業」。
- 異なる作業ディレクトリからStartGame.cmdで起動。初期Day1/300G、ニンジン種5を確認。
- 一覧は「企業向け体験用 種各5個」の1/1。JSONで3種類各5個・旧カブ0・大会モードを検証。
- 検証コピーだけで畑0を耕し、保存、終了、ランチャーで再起動。畑0の「耕し済み」を復元確認。
- 検証用プロセスは終了。配布フォルダー/ZIPは未プレイの初期セーブ1件を維持。
- 元の開発用Settingsはパッケージ作成前後でハッシュ一致。元のセーブや他ブランチは変更しない。

## 未実施・制限

Debug再ビルド、全30日通しプレイ、別PC/GPU/DPI、起動失敗ダイアログの故障注入は未実施。
540G金額クリアなし・10/20/30日大会あり。日数無制限モードではない。
通常のRestartでは種0個に戻るため、初期セーブを読み込むか別フォルダーへZIPを展開し直す。
素材出典と利用許諾の確認は未完了。NOTICES_PENDING.txtを維持し、企業への提出済み・全規則適合とは扱わない。
ブランチ個人就職作品、HEAD f68a0b5f1175b83763aa0fec8e126a324c3b53ce。stage/commit/push/merge/uploadなし。
旧 `SuidoNogyo_Runtime_20260928.zip` はCG2表示の改名のみ候補であり、02版に置き換わる。
