# 波紋遷移の検証 2026-10-07

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
