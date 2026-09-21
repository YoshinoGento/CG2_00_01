# 畑ごとの自動給水: 検証記録（2026-09-19）

## 対象と責務

- 通常の土マスへの自動給水を個別に停止・再開する。水源の栓、排水、適正水分の自動維持ではない。
- FarmTileが設定を保持し、FarmToolActionSystemがCommandHistoryへ設定変更を登録する。Undo/Redoは設定のみを復元し、時間経過した成長・水分を巻き戻さない。
- FarmIrrigationSystemは停止した畑を実際の需要計算と給水予測から除外する。物理的な配管接続・範囲は維持する。
- Bridgeは世代・プレビュー・終了状態を検証し、未保存管理を更新する。UIは状態表示と要求通知だけ。観察中の取水変更は比較条件の変更として検出する。
- 通常保存schema15／配置保存version2に記録する。旧schema1〜14／配置version1は取水ありへ移行する。欠落やboolean以外は現在の状態を変更せず拒否する。

## 自動検証

リポジトリルートで各cmdを実行する。専用テストは `/W4 /WX`。

| コマンド（tools/tests/） | 結果 | 主な確認 |
|---|---|---|
| run_farm_irrigation_intake_test.cmd | 合格 | 1/2/4倍速の給水と水収支、停止マスのみ受水0、他マスの継続、残水保持、再開、上り坂拒否、手動水やり、設定だけのUndo/Redo、範囲外・水路・水源・空Grid拒否、世代変更、古いプレビュー、比較無効化 |
| run_farm_runtime_view_test.cmd | 合格 | 入／切の要求値・選択色・操作不可、当たり判定、各要素の重なりと範囲、ラベル最小寸法、HUDとの分離 |
| run_farm_layout_test.cmd | 合格 | 配置設定の往復保存、version1移行、不正値の原子的拒否 |
| run_farm_crop_size_document_test.cmd | 合格 | 通常保存・再オープン・旧形式移行、不正値の原子的拒否、リセット、従来の大会・収穫・日付保存 |
| run_farm_care_history_test.cmd | 合格 | 品質履歴の保存と旧形式移行。コンソールのschema12表記は古いが、保存形式の検証は15 |
| run_farm_height_brush_test.cmd | 合格 | 地形ブラシ、補間、Undo/Redo、古い候補の拒否 |
| run_farm_play_loop_test.cmd | 合格 | 売買・収穫・保護・大会・進行の既存ケース |
| run_farm_season_balance_test.cmd | 合格 | 既定の給水ありを含む36ケース。新しい手動切替戦略の遊びやすさを証明するものではない |

Debug x64／Release x64のビルド成功。既存のchild-cmdラッパーでPath/PATH重複を回避した。

## 描画資源・安全性

- 同じラベルTextureを4列へ再配置し、271項目・2560×3120とした。以前の1920×4088からベースRGBA換算で約0.54MiB増。Texture枚数、Descriptor数、Barrier/Fenceの所有権は変更していない。
- 取水処理に追加するのはbool判定。各フレームの新規動的確保は追加していない。操作時のCommand確保は既存方式。
- Gridを所有しないCommandは世代とindexを確認する。履歴はGridより先に破棄されるScene内の所有順序を維持する。
- GPU負荷や全フレームのCPU負荷は未計測。

## 実画面確認の阻害要因

元の保存を変更しない `generated/codex_checks/intake_runtime_20260919` を準備したが、今回の環境ではReleaseが画面初期化中にアクセス違反となった。CDBで例外停止して確認した呼出し順は、WinMain → Game::Initialize → Framework::Initialize → DirectXCommon::Initialize → InitializeCommand。

既存の `DirectXCommon.cpp` はDevice作成をFeature Level 12_2に固定し、HRESULTを確認せず次のInitializeCommandへ進む。今回の停止はnull Deviceの参照で、農場システムの初期化より前。GPU非対応・ドライバ等の生成失敗理由の詳細までは未確定。今回この低レイヤコードは変更していない。

したがって、新UIの実ゲーム上の可読性、実クリック、ImGuiの表示、操作から保存して再起動する一連の確認は未完了。自動テストを録画・実操作の証拠として扱わない。次はDevice作成失敗の処理と対応Feature Levelの方針を確認してから、Release 1280×720／1920×1080で確認する。

## 保存データと提出物

撮影用9/13 runtimeのResourcesが開発用ResourcesへのJunctionだったため、今回変更が反映されたPNGは共有を切り離した上で元のGit版へ復元した。共有リンクの退避先は `generated/codex_checks/weekly13_resources_link_20260919`。撮影用Settings・動画・週次報告は変更していない。対象保存のSHA256は作業前後で一致。

コミット・ステージ・プッシュ・マージ・他ブランチ操作なし。
