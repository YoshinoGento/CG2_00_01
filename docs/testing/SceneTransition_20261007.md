# 波紋遷移の検証 2026-10-07

## 2026-10-13 学校向け画面遷移提出版
- 提出ZIPはgenerated/transition_contest_20261013/ready。Release x64、実行時ファイルだけを含む約48.1MB。初期セーブはDay1/300G/各種5個のFreeFarming、開発・検証セーブは含まない。
- 原本Settingsを使わず、日本語・空白のパスへ展開したコピーで起動。タイトルクリック、種屋の出入り、商品選択、購入確認/取消、展示台の出入り/一覧を確認。展示は空のため収穫物の内容検証ではない。
- トマト購入で300Gから210G、種5個から6個。保存・正常終了・再起動後の同じ値を画面と保存JSONで確認。連続クリックでは次の購入確認も開いたが取消し、2回購入はしていない。
- 動画40秒、H264/1280x720/30fps、AACゲームプロセス音声、約3.2MB。タイトル→開始案内→畑、畑↔種屋、畑↔展示台の6区間。待ち時間のみ編集し遷移は等速、音や遷移効果の後付けはない。
- 動画全編デコード、全体1fps/タイトル6fps/元の種屋・展示4fps画像を確認。音声非ゼロ、ピーク-16dBFS。録画の最初のフレーム遅延や主観的な聴感は独立計測していない。
- ZIPのCRC・メンバー集合・全ハッシュ、初期納品フォルダ・原本Settings・以前のZIP不変を確認。独立QAは正常終了。今回の提出物をアップロードしたわけではない。
- 別PC/GPU/DPI、実キー長押し/Gamepad、全ゲーム内容、GPU性能は未検証。自動状態テストとローカル実機確認の範囲を混同しない。

## 2026-10-13 種屋の短い暗転と寄り引き
- 種屋の出入りだけCover0.18秒/Reveal0.26秒。画面全体の画像変換であり実Camera移動ではない。
  タイトル/展示/帳簿の波紋、商品/数量/確認/キャンセルの即時操作は維持。
- Debug/Release x64 exit0、DXC ps_6_0 -WX -Zpr exit0。
  Shader reflectionのcbuffer128 bytes、画像scale/fade/branch/reservedが112/116/120/124 bytes。
  C++の32 floats/Root Signature33 DWORDsと一致。
- 状態テスト:100回交互の入退店、4種類のdelta、単調なfade/画像scale、完全暗転後の切替、
  exact-once/入力解放待ち/未知style拒否/SceneReplacement拒否/水滴なし/音の重複なし/波紋へ復帰PASS。
  既存の遷移100回、種屋/Runtime View/展示/音量設定/PCM素材/波紋960+反射2424参照もPASS。
  実GPU時間は未計測。CPU参照テストをShader実行結果とは扱わない。
- 独立ShopShortQA.exeはReleaseとSHA327AAC674B1E8956F909B4F18A21077D5421F066E2F1E62B3822A8D4DE7AADE1が一致。
  PSは88C75B2C690F9F159DBA0E72C028D4665271A9875C4DB0FB0C5AE1E07F25AAAC。
  通常1280x720の入店/退出ボタン/右クリック退出、商品切替、確認/取消、461G->401G/種4->5を確認。
  最大化で種屋の入退店とHit Rectを確認。exclusive fullscreenの検証ではない。
  タイトルの戻り波と展示入退出の波紋も残っている。
- 証拠: generated/codex_checks/shop_short_transition_20261013/shop01.mp4、119.983秒/5637frames/H2641280x720/音声なし。
  全デコード成功。entry_detail01.png（26.35秒から20fps）で暗転/不透明な切替/店への寄りを確認。
  shop_preview01.mp4は24.7秒から4.5秒の抜粋で全デコード成功。
  暗転色は後段LinearToSRGBで明るさ変換されるが、切替フレームは画像が見えない不透明な単色。
- QA17856は通常Alt+F4/Wait exit0、elevated process readで不在確認。
  原本Settingsの全ファイル/ハッシュ、提出ZIP SHA79F317E95DA3FA34A9355718BA3F0CD79915FF7C250EDD4D195AE010D133B572は不変。
- 比較確認: 直前のEXE534B19.../旧PS E62667...でも、種屋へ入る前の畑に同じ白い帯が出る。
  本変更以前の表示であり、原因/修正は別課題。通常1280x720で確認。
  最初の比較9228は別アプリへ切り替わったため操作を停止し許可を確認。その後終了、不在確認。
  ユーザーの続行許可後に29312で比較を完了し、Alt+F4/Wait exit0、検証プロセス不在を確認。
- 残課題: 畑の白い帯の原因、実キー長押し/Gamepad/別GPU/DPI/描画失敗/GPU計測。
  展示/帳簿の専用演出は未実装。

## 2026-10-13 遷移SE試作
- 実装: タイトルの着水/広がり/戻り波、種屋の紙、展示の木台、畑へ戻る擦れ、一覧の紙めくり。
  7種類の固定seedオリジナル合成WAV。音量gain0.30、既存環境音/水音スライダーとは独立。
  SEスライダーと行き先別の画面演出は未実装。背景プレビュークリックには音を追加していない。
- Debug/Release x64 exit0、状態テストの0.016/0.1/0.25秒・大きなdeltaでcue一回性、
  描画済みCoverより前にはReveal音が出ない条件、4テーマ100回再入場がPASS。
- PCM検査: mono/48kHz/PCM16、hash/再生成一致、peak約0.52、RMS0.077～0.122、
  端点0・edge/DC・合計276480 bytes・プロジェクトXMLの登録を確認。
- XAudio2/MF integration: null/途中読込失敗の後始末、7clip/552960 decoded PCM bytes、
  Voice上限4・100回重複要求・自然終了・3回再入場・別ownerのClip/Voice保持がPASS。
  間違いなくPCMを再生/解放した検査であり、聴感評価ではない。
- 既存の音量設定、種屋、展示、Runtime View、波紋960+反射2424の回帰PASS。
- 独立Release: generated/codex_checks/transition_audio_20261013/runtime01/TransitionSoundQA.exe、
  ReleaseとSHA534B19AEC2CA03953EBA22A25F896E6AD5632FDC445AF4EFC11FCCED03564BA9が一致。
  コピーだけ環境音/水音を0にし、タイトル開始・種屋入場・畑戻り・展示入場・一覧移動をクリック確認。
  process限定WASAPI meterで各経路のnonzero peak0.155997、遷移外の無音peak0を確認。
  title_peak02/shop_peak02/return_peak/display_peak/ledger_peak.txtが出力検査の記録。
  初回restricted probeは出力できず、古いscreenshotIdの試行も未操作。成功した再試行と区別する。
- 2回の検証起動はAlt+F4で正常終了、両wait exit0、ゲーム/検査process不在確認。
  元Settings全ファイルのSHA256と提出ZIP SHA79F317E95DA3FA34A9355718BA3F0CD79915FF7C250EDD4D195AE010D133B572は不変。
- se_preview.wavは7個を実装gain0.30で並べた試聴用音声（ゲーム録音ではない）。
  音の好み/他スピーカー/別PC・GPU/DPI/Gamepad/巻戻し中のglobal temporal影響/SE bus分離は未検証。
  DX12 Renderer/資源・同期経路はこの音声追加では変更していない。

## 2026-10-13 種屋・収穫物展示への出入り
- 実装: Scene交換せず、不透明Coverの描画後にControllerの保留Requestを1回だけ実行してReveal。商品選択・数量・確認・取消は即時。展示から一覧・大会・Tabへの退出も同じ経路。
- 自動確認: Debug/Release x64、遷移100回交互のscene/screen・描画済みゲート・exact-once・入力解放待ち、種屋・展示・Runtime View、波紋960+反射2424参照テスト成功。Renderer/Shader資源経路は今回変更なし。
- 独立Releaseの実機確認: 種屋クリック入店・退出ボタン・右クリック退出、展示台入場・一覧への移動・畑への退出。購入確認/取消はその場で表示、購入で461G→401G・ニンジン種4→5。旧記録3個には個別品質がなく、展示が空という既存仕様は変更していない。
- 証拠: generated/codex_checks/shop_transition_20261013/shop01.mp4（150秒、全デコード成功）、shop02.mp4（120秒・1280x720・H264・5444フレーム・音声なし）、entry_wave01.png（12fps）、shop_preview01.mp4（7秒）。戻り波の通過後の外側から店内が表示されることを確認。
- 未確認: ESCの自動入力は退出を確認できず、実キー長押し/Gamepad/別GPU/DPI/描画失敗fallback/大会・Tab経路/実GPU計測は未検証。最終Alt+F4操作時に物理EscでComputer Useが停止されたため、以降のUI操作を中止。検証ゲームの正常終了、shop02の全デコード、元Settingsの最終ハッシュ照合は未完了。既存提出ZIPのハッシュは変更なし。

## 2026-10-13 反射波によるセレクト画面のReveal
- 最終実機確認: 独立Releaseで背景クリックは波紋のみ、開始クリックは外向きCover→不透明待機→内向き反射波→セレクト画面。通過後の外側だけが表示され、中心の覆いが縮む。3重の屈折・局所ぼかしを12fpsコマ送りで確認。次画面はクリックを持ち越して選択を実行せず待機。
- 証拠: generated/codex_checks/water_transition_20261013/reflected03.mp4（30秒・1280x720・H264・1371フレーム・音声なし）、reflected_contact03.png、reflected_detail03.png、7秒のreflected_preview03.mp4。両動画は全デコード成功。ゲームは正常Alt+F4で終了、プロセス不在確認。
- 起動/撮影上の注意: GUI起動後に終了を待つ方式と通常ウィンドウで撮影が完了。Hiddenで起動した専用プロセスはウィンドウを取得できず終了処理した。gfxcaptureのd3d11画像をhwdownload,format=bgraで変換。reflected01/02は失敗した試行で証拠ではない。
- 外向きCover後、外周からクリック位置へ戻る3重波と、通過後の外側だけを見せるマスクを実装。Revealは0.9秒。
- Debug/Release x64、DXC PS6_0 -WX -Zpr成功。状態テスト100回反復・入力解放・描画済みCover、設定入力の回帰PASS。
- CPU参照: 既存960サンプルに加え、中央/四隅のクリックと各画面点で2424サンプル。
  内向き半径、通過前の非表示、通過後の表示、単調な可視範囲、有限値、両端の波振幅0を確認。
  これはShaderの実行結果ではない。
- 独立WaterReflectedQA.exeの起動ログでタイトル初期化を確認。アプリ操作の承認がタイムアウトし、最終クリック撮影は未実施。
  この中断は上記の最終撮影で解消。古いwipeの動画を本変更の証拠にしない。
- 元Settingsの全ファイル/ハッシュと提出ZIPは変更なし。今回の専用QAとデバッガは終了済み。
- 同日続行: EXE/PSと独立コピーのSHA256一致を確認し再起動。手動入力を検出し、再取得画像では既にセレクト画面だったため、こちらの開始クリック撮影は未実施。再起動して30秒撮影するかユーザーの確認待ち。状態/波紋参照テストは再度PASS。手動操作された検証ウィンドウは閉じず残している。
  その後ユーザーから続行の指示を受け、上記の最終撮影と正常終了を完了。待ち状態は解消済み。

## 追加した水滴・屈折版
- クリック位置から3重の波、落下する水滴、背景の屈折と局所ぼかしを実装。
  背景クリックはプレビューのみ、開始クリックは同じ中心から遷移。設定中のクリックは消費する。
- 最終Debug/Release x64 exit0、dxc VS/PS -WX成功。
- SceneTransitionSystemの4滴上限/寿命/中心/範囲/100回遷移/描画済みCover/入力解放、
  Title設定のクリック通知/モーダル/保持/保存、Title600秒と画像回帰が成功。
- water_ripple_math_test.py:960個のCPU参照サンプルで有限値/端clamp/中心/4波重複/寿命/3つの外向き波を確認。
  Shader実行結果の検査ではなく、実画面の代わりにはしない。
- 独立QA: generated/codex_checks/water_transition_20261007/runtime01/WaterTransitionQA.exe。
  最終ReleaseとSHA256が一致:488A87B4229D2EA45D031D26A1AC70DB19DE76A613951C303D1EEAFBB1DE45EA。
- 最初のQA32916はウィンドウ一覧に現れた後、画像取得/録画前に不在となった。
  対応するApplication Errorは見つからず終了原因は不明。録画water01はwindow不在で失敗。
  最終実行版の水滴・屈折・ぼけ・開始クリックは未確認。再起動確認の返答待ち。
- 原本Settingsの全ファイル/ハッシュと既提出ZIPのSHA256は変更なし。

以下は先行した円形wipe版の検証であり、屈折版の実機確認結果ではない。

## 対象
TITLE -> GAMEPLAYの導入画面。ショップ・メニュー遷移は未実装。
検証コピーはgenerated/codex_checks/scene_transition_20261007/runtime01。
原本のSettingsと提出済みZIPを使った起動・保存は行っていない。

## 自動確認
- 最終Debug/Release x64 Build: exit0。
- SceneTransitionSystem: 描画済みCoverを必須にする交換、再要求拒否、100回反復、有限delta、入力解放待ちPASS。
- TitlePresentationSystem: 600秒の空・作物・波紋・点滅、開始通知の一回性、起動直後の開始でもentry fadeが連続する回帰PASS。
- 音量設定・クリック範囲・保存失敗/破損設定の回帰PASS。想定した失敗ケースの警告はテスト出力に含む。
- テキスト画像/波紋マスク、Shader vs/ps_6_0 -WX、プロジェクトXMLの重複登録、diff --check PASS。

## 実画面
- 最終Releaseの独立コピーを1280x720で起動。「クリック / SPACE / ENTER / A」と点滅する開始表示を確認。
- 「はじめる」をマウスでクリックし、外向きCover -> 完全不透明な待機 -> 外向きReveal -> フリー農業導入へ進んだ。
- 導入UIは開いた時点で表示され、開始クリックだけで導入の選択肢まで実行されなかった。
- transition03.mp4: 31.25秒、H264、1280x720、1875フレーム、音声なし。全デコード成功。
- contact03.png(2fps)とdetail03.png(12fps)を確認。transition_preview03.mp4は14秒地点から6秒の抜粋。
- 35秒の録画指定に対し31.25秒で終了し、その後プロセスは不在。最終実行に一致する新しいApplication Errorは検出していないが、終了原因は断定しない。

## 修正した不具合
修正前のQAは開始直後に0xc0000005で終了した。
Windowsの例外位置0xb70f6を当該ReleaseのPDBで調べると、ParticleManager::Drawのカメラ参照に一致した。
遷移中のUpdate停止によって、従来の「Drawより先にUpdateを呼ぶ」前提が成立しなくなったため。
GamePlayScene::PrepareRenderFrame(0)とParticleManager::Update(camera,0)で初回描画だけを準備し、
DrawのnullptrガードとFinalizeでの借用解除を追加。修正後は上記の導入画面へ到達した。
transition01/02は修正前の記録なので、最終品質の証拠に使用しない。

## 未検証
最大化・別GPU/DPI・実Gamepad・キーを押し続けた実機試験・長時間プレイ・GPUプロファイル・Shader失敗注入。
入力持越し対策はCPU状態テスト済みだが、すべての実入力機器での確認ではない。
Sceneの初期化は同期処理のまま。不透明待機の長さや総遷移時間を保証しない。
ParticleManager全体の所有権や既存GPU処理を網羅的に検証したわけではない。
