# 農場追従カメラ検証 2026-09-24

## 目的と設計

以前の追従はプレイヤー中心の固定offsetで、畑が日付・所持金HUDに隠れていた。農業ゲームモードだけ、農場とプレイヤーを既存HUDのsafe areaへ収める。

- FarmOverviewCamera.h: FitOverviewCameraに既定0.70radの検証済みpitch引数を追加。新しいFitFarmFollowCameraは農場boundsとプレイヤーCollider＋0.25余白のunionを0.35radで透視投影fitする。固定サイズのスタック配列、失敗時は出力不変。
- FarmRuntimeController::FrameFarm: 既存の農場範囲・モード別safe areaを共有し、追従時だけplayer boundsを渡す。追従ではdebug cameraの退避値を上書きしない。
- GamePlayScene::UpdatePlayerCamera: 農業モードはControllerへ委譲。一般の開発用追従は従来offsetを維持。UIは表示／入力通知のままで、資源・ゲーム状態の所有権変更なし。

## 検証済み

- run_farm_runtime_view_test.cmd: 合格。162組（3画面比率、3モード、9平面位置、2高さ）で農場とCollider余白の全隅がsafe area／Near-Far内。不正extent／NaN／逆転bounds／clip不足／不正pitchは出力を変更しない。既存overview・HUD入力境界テストも合格。
- 最終Debug／Release x64ビルド成功。
- 独立した `generated/codex_checks/follow_camera_20260924` の合成農場でRelease確認。1280x720と最大化1920x1008の双方で、追従時の畑・作物・プレイヤーが常設HUDに隠れない。
- 停止中の追従切替、クリックによる0番→1番の選択、選択だけでは構図不変、最大化後の見渡し復帰を確認。最大化はOSメニュー経由で行い、停止／1xを維持した。
- 初回0.70radの共通角度では遠方の農場が小さすぎたため、追従のみ0.35radへ変更して再ビルド・再確認。

## 制約と未確認

- 農場から離れるほど畑が小さくなる。近接三人称から、農場と人物を同時に見る構図へ仕様を変更した。細かな作業は見渡し表示を使用。
- Collider余白は現在のキャラクター用の保守的な近似であり、任意モデルの全アニメーション頂点を包含する保証ではない。作物は既存0.8の高さ余白、地形は編集範囲内を想定する。
- 実画面でWの短い入力を行ったが、長距離移動・ジャンプ中の連続追従は未検証。位置の網羅はCPUテストで確認。Debug実画面、極端な遠距離、遮蔽物回避、補間／揺れ対策は未検証・未実装。
- 前回観測した最大化ボタンのクリックによる速度変更の原因修正は行っていない。今回のOSメニュー経由の成功と混同しない。
- 新規GPU資源・Descriptor・DrawCall・Barrier／Fence変更なし。GPU／CPU時間の実測は未実施。元のセーブは変更せず、合成農場は週次報告の実績証拠ではない。
