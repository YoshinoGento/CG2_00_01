# プログラム説明資料の根拠と本人確認

2026-09-28。提出用本文ではなく、説明内容をソースへたどるための作業資料。
本文の正本は `SuidoNogyo_ProgramGuide.md`。図とPDFは `generate_program_guide.py` で生成する。
出力は `generated/kurihaku_20260928/program_guide/プログラム説明資料（水道農業）.pdf`。
現時点では本人の表現・理解の確認前。既存の実行版ZIPには追加していない。

## 本文と実装の対応

| ページ | 確認した根拠 | 説明の範囲 |
| --- | --- | --- |
| 1 | 実行版04の初期保存、PROVENANCE_VERIFICATION.md | フリー農業、5×4、正式作物3種。学校基盤を利用した個人作品。 |
| 2 | project/application/farm/core/FarmGrid.h、project/application/editor/GamePlayEditorBridge.cpp、project/application/scene/GamePlayScene.cpp:834 | 主要操作経路とFixedUpdateの順序。SceneやBridgeの完全な責務分離とは書かない。 |
| 3 | project/application/farm/system/FarmIrrigationSystem.cpp:54 / Rebuild | 四近傍、訪問済み、高さ制約、先着の上流1つ。再構築は給水更新ごと。 |
| 4 | 同ファイル / UpdateWater、balanceError:390 | 水源補給後のスナップショット、水路移動の差分、畑の要求合計と比率配分。図の0.4/0.3は説明用の数値。 |
| 5 | project/application/farm/system/FarmIrrigationPreviewSystem.cpp:365 / CanConfirm、GamePlayEditorBridge.cpp:707 | 世代・寸法・各マスの照合。確定後はToolActionSystemへ。プレビュー全体コピーのコストも記載。 |
| 6 | project/application/farm/system/FarmCropQualitySystem.cpp:82 / Evaluate、:22 / Analyze、project/application/farm/data/FarmRules.h | 適正水分3種、履歴平均、4軸、助言、品質価格。実際の栽培知識や大会合計とは区別。 |
| 7 | tools/tests/farm_irrigation_intake_test.cpp、run_farm_irrigation_intake_test.cmd、run_farm_runtime_view_test.cmd、PROVENANCE_VERIFICATION.md | 今回再実行した機能テストと、先行する同PC配布版試験を分ける。 |
| 8 | 上記コード、ASSET_CREDITS.txt、実際の制作対話 | 今後の改善・学校基盤・OSS・AI支援。全コードを独力で制作したという表現はしない。 |

## 今回の確認

- 灌漑テスト: `cmd /d /c tools\tests\run_farm_irrigation_intake_test.cmd`、終了コード0。
  結果: `PASS: intake delivery/conservation 1x2x4x, manual water, history, migration state, stale preview and comparison`。
- 表示データ・カメラテスト: `cmd /d /c tools\tests\run_farm_runtime_view_test.cmd`、終了コード0。
  結果: `PASS: follow camera farm/player safe bounds, 162 poses, invalid input retains pose` および
  `PASS: hit boundaries, disabled actions, navigation, capacity, atlas bounds`。
- 上記はMSVC/C++20/x64、/W4 /WXの機能テスト。フレーム時間やVRAM計測ではない。
- 8ページをPDFからPNGへ描画して目視確認。本文31枠の高さ・ラベル横幅、8ページのタイトル抽出、埋め込み日本語フォントをスクリプトで確認。
- 最初の可変フォントは既定ウェイトが細すぎたため、PDFのみMeiryoのRegular/Boldを埋め込み、再描画して確認。ゲームのフォントは変更しない。

## 掲載画面

- 実行版04を `generated/kurihaku_20260928/program_guide_scene_02` に展開した説明専用コピー。
- `prepare_program_guide_scene.py` が水源・用水路・成長中作物を保存JSONに設定。自然な通しプレイの成果ではない。
- 初版の撮影コピーではtimeScale=0が保存形式の許容値1/2/4に一致しなかったため不採用。02では1を使用し、起動後に画面から時間を停止。ゲーム側のコード修正はしていない。
- `farm.png`: ニンジン選択と水路、3作物を表示。
- `quality.png`: かぼちゃ#13の品質予測。成熟99、水分36、地形100、養分90、品質72、予想単価513Gの実表示。
- 水過多の状態もそのまま掲載。良好な栽培結果や価格の最適解を示した画像ではない。
- 両画面はトリミング・数値加工なし。PDF内で縮小配置。撮影後はこのコピーだけ終了。

## 提出前に本人が確認すること

1. 「作りたい遊び」「この方法を使う理由」が本当に自分の意図と合っているか。異なる部分は、自分が実際に考えた言葉へ直す。
2. 0.4の水を0.3ずつ要求する2マスへどう配るか、プレビューを別データにする理由、水圧計算ではない理由を口頭で説明できるか。
3. 本人が担当・判断した範囲と、授業基盤・AI支援・外部素材の範囲に誤りがないか。本文は独力で全て作ったとはしていない。
4. 実際の苦労や比較した案があれば、具体的な事実として追加する。試していない案を「比較検証した」と書かない。
5. 氏名・学校名・学科・制作期間は、確認済みの正式情報だけで最終表紙や提出名へ反映する。今回は未確認のため追加していない。

## 保全・未確認

- 実行版04のSHA-256は `dcd01a745ca9e7f140d5b5d661950f946e4881ea93ee6339be691239afe66042` のまま。
- 本番ソース、開発セーブ、承認済み配布ZIPは今回変更していない。
- 個人就職作品 / HEAD `f68a0b5f1175b83763aa0fec8e126a324c3b53ce`、stage空。commit/push/merge/外部アップロードなし。
- 本人の強調したい経験の回答は未受領。別PC、全DPI、30日通し、性能測定、DirectX 12全体監査は未実施。
