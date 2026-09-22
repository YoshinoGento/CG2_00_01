# プレイ中の速度操作 検証記録（2026-09-21）

## 対象と責務

通常プレイの上部HUDに既存の1倍／2倍／4倍操作を追加。日付スキップ、上限倍率、価格、成長式、1日60秒の規則は変更しない。

- FarmTerrainView: 固定寸法の3ボタンと選択状態、Speed要求。
- FarmRuntimeController: 対応する整数1/2/4のみ受理し、既存と同じ停止解除と未保存管理を適用。
- FarmDateSystem / FarmContestDaySystem: 既存の速度保持と大会日境界停止を維持。
- GamePlayEditorBridge / FarmControllerWindow / EditorLocalization: 時計Snapshotと日長の読み取り専用表示。
- ラベルgenerator、FarmRuntimeLabels.h、farm_runtime_menu.png: 日本語の短い倍率表示。

## 自動検証

- `tools/tests/run_farm_runtime_view_test.cmd`: 合格。停止・編集可否・時間操作可否・各倍率・未対応値/NaNについて、ボタン数、選択状態、クリック範囲、無効操作、文字幅、HUDとの非重複を確認。
- `tools/tests/run_farm_crop_size_document_test.cmd`: 合格。1/2/4倍の進行量とSnapshot復元、不正倍率拒否、Day9から4倍でDay10の境界停止、案内待ち中の倍率変更でも日付が進まないことを追加検証。既存の10/20/30日目と保存互換性の検証も合格。
- Debug / Release x64: 最終ビルド成功。既存のchild-cmdラッパーを使用。

## 実画面で確認したこと

`generated/codex_checks/intake_visible_20260921` の隔離Settingsを使用。元セーブへの日付・水分・品質の直接書き換えは行っていない。

1. Release、client1280x720: 上部に1倍／2倍／4倍が収まり、他HUDと重ならない。2倍への切替で左HUDがx2になった。
2. 一時停止でボタンが「時間を進める」に変わり、倍率の選択色が外れた。4倍を押すと再開し、左HUDがx4になった。
3. 最大化、client1920x1008: 速度ボタンと周辺HUDの文字・配置を確認。
4. Day15から通常の4倍進行でDay20の大会案内を表示。「畑で準備（停止を維持）」で戻り、Day20と停止状態を確認。倍率操作で未確認の大会日を飛び越さなかった。
5. 1倍へ戻すとx1になり、停止操作も有効。通常メニューからQA記録へ上書き保存し、「保存済み」を確認して終了。
6. Debug再起動: 同じQA記録からDay20、日内8.6/60秒、設定1倍を表示。大会案内による停止と、明示的一時停止の表示を確認。保存済みのまま終了。

Release PID33388とDebug PID32484は終了し、CG2プロセスが残っていないことを確認した。

## 安全性と負荷

固定長配列と既存View/Spriteプールを使用。追加は最大3背景と3ラベルで、ラベル数285から288に対し既存atlasの空き枠を使うため2560x3296の寸法は変わらない。新しいTexture、Descriptor、Barrier、Fence、所有権や寿命の変更はない。GPU時間、DrawCallの実測、全解像度の確認は未実施。

## 制約と残課題

- 大会日10/30の新ボタンからの実画面確認は未実施。今回の実画面確認は20日目で、自動テストと区別する。
- 30日間の人による通しプレイ、待ち時間の妥当性、難易度調整は未完了。4倍では水分消費や自然給水も速くなる。
- 週次報告用動画は撮影していない。この記録を映像による成果実証や採点に置き換えない。
- Debugコンソールの既存エラー3件・警告2件は残っており、今回診断していない。
- 既存のDX12起動修正・給水ガイド等の未コミット差分は保持。個人就職作品のHEADは0c2b7deed98c28c48f15489be23bd0325b65cd62のまま。stage/commit/push/merge/GitHub操作なし。
- submission配下の追跡差分なし。9/13原本セーブのSHA256はE3ABFFCC6C5168C9F1C31B01744FE7515D49BFBF47389D8BAACD859A2F5F9F4Eで不変。
