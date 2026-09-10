# 農業ゲームRelease UIの調査と採用判断

調査日: 2026-09-09。目的は録画用Releaseでも、農作業・地形編集・記録・観察を迷わず扱えること。競合作品の素材やプログラムは転用しない。

## 実際に確認した資料

### 9/9追補: DirectInputの短いクリック

[Microsoft: Buffered and Immediate Data](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/ee416236(v=vs.85))は、現在状態のスナップショットとイベント履歴を区別し、ボタンにバッファ入力、軸に即時入力を組み合わせる方式を説明している。現行のGetDeviceStateだけではフレーム間に押して離した状態を失うため、マウスボタンをバッファ取得へ変更した。キーボード・ゲームパッドは対象外。

[Microsoft: GetDeviceData](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/ee417894(v=vs.85))と[SetProperty](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/ee417929(v=vs.85))に従い、DIPROP_BUFFERSIZEを設定し、取得結果とDI_BUFFEROVERFLOWを確認する。復帰・欠落時はバッファを破棄して同期し、古い操作を再生しない。追加した純粋なMouseButtonEdgesは一フレームの押下・解放・保持を分離する。複数回のクリックは一回へ集約する既存APIを維持し、全クリックの履歴再生は行わない。

### 9/9追補: DirectInputの短いクリック

[Microsoft: Buffered and Immediate Data](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/ee416236(v=vs.85))は、現在状態のスナップショットとイベント履歴を区別し、ボタンにバッファ入力、軸に即時入力を組み合わせる方式を説明している。現行のGetDeviceStateだけではフレーム間に押して離した状態を失うため、マウスボタンをバッファ取得へ変更した。キーボード・ゲームパッドは対象外。

[Microsoft: GetDeviceData](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/ee417894(v=vs.85))と[SetProperty](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/ee417929(v=vs.85))に従い、DIPROP_BUFFERSIZEを設定し、取得結果とDI_BUFFEROVERFLOWを確認する。復帰・欠落時はバッファを破棄して同期し、古い操作を再生しない。追加した純粋なMouseButtonEdgesは一フレームの押下・解放・保持を分離する。複数回のクリックは一回へ集約する既存APIを維持し、全クリックの履歴再生は行わない。

### Farming Simulator 22公式Build Mode紹介

[公式記事](https://www.farming-simulator.com/newsArticle.php?country=ca&lang=en&news_id=251)は建物配置・地形編集・撤去の切替と、カテゴリ分けを説明している。柵は起点を置いて引き延ばす操作。

[公式の柵配置画像](https://www.farming-simulator.com/cms/uploads/news_60fee4bb06a61.jpg)をブラウザーで表示して確認した。中央に配置中の柵、下にカテゴリと品目、右上に日時・所持金、左上に確定・取消などが分かれている。選択品は明るい色で強調され、価格も同じ一覧から読める。

[公式動画: Preview: The new build mode in Farming Simulator 22](https://www.youtube.com/watch?v=FtkMPrF6VlQ)を公式記事内で再生し、冒頭付近、6～7分台、10分台の一部画面を確認した。建築一覧と農場俯瞰、説明者を挟んだ実演構成、確認メッセージ画面を確認。10:19付近では地面上のブラシ範囲と選択中のLandscaping、10:28付近では地形ツールの一覧を確認した。全12分22秒の連続視聴や、全機能の操作検証はしていない。静止画で見える配置と、動画中で確認できた状態を混同しない。

### Stardew Valleyの操作体系

[Controls](https://stardewvalleywiki.com/Controls)と[Inventory](https://wiki.stardewvalley.net/Inventory)の説明を参照した。頻繁に使う道具をツールバーへ置き、持ち物画面と分ける考え方を参考にする。この資料はコミュニティWikiであり、今回この作品の動画実演を確認したという意味ではない。

## このゲームで採用する判断

| 課題 | 採用する形 | 判断理由 |
| --- | --- | --- |
| HUDと開発UIが農場を隠す | 通常HUDとEscメニューを分離 | 作業中は畑を見せ、詳細は必要時に開く |
| ショートカットを覚えないと操作できない | 日本語の5カテゴリとクリック操作 | 農作業・売買／地形・用水路／記録／観察／時間・カメラの目的別に探せる |
| メニュー操作が畑に漏れる | 入力の排他と時間停止 | 道具を選ぶクリックで背後の畑を変更しない |
| 地形変更を誤って確定する | 既存プレビュー＋確定／取消 | 見た結果を確認してから変更する |
| 細かな数値が読みにくい | 日本語ラベルと数値の列を分ける | ラベル幅を制限し、長い説明が数値へ重ならない |
| セーブの選択違い | 現在・選択中の名前、日本語入力、読込前確認 | 9/9追加要望に合わせ、名前で判別できる一覧へ変更 |
| 動画で生育差を説明しづらい | 観察A/Bと品質値をReleaseへ公開 | 開発画面なしでも比較と結果を示せる |

これらは参考作品そのものの仕様ではなく、本作の既存実装に合わせた設計判断。自動保存、巨大な建築カタログ、装飾品の追加は今回採用しない。

## 実装責務と確認事項

### 追補: 観察のメニュー往復を減らす

[Factorio公式 FFF #380](https://www.factorio.com/blog/post/fff-380)は、場所ごとのGUIを開閉して切り替える操作の煩雑さを説明し、実際に操作できる遠隔画面へ対象切替を統合している。本作ではこの考え方をA/B選択に応用する。ノートを開き直さず畑を選べる構成は本作向けの設計判断であり、Factorioの生育比較機能を再現したという意味ではない。

[Farming Simulator公式のCrop Sensors紹介](https://www.farming-simulator.com/newsArticle.php?country=gb&lang=en&news_id=343)も参照した。作物状態の計測値を作業の判断につなげる方向を参考にするが、本作へ窒素・pH・センサーの仕組みを追加するものではない。

- 添付の10.90秒動画は3秒/10秒の抽出フレームと別添スクリーンショットを確認。動画全編の操作検証ではない。既存画面の状態と今回の追加機能の動作証拠を混同しない。
- 採用: 登録後に時間停止した畑へ戻る、次の観察枠を直接選ぶ、A/Bの数値と時間操作を同じ画面に置く。農作業への入力漏れを防ぎ、従来ノートは残す。
- 不採用: 全HUDの作り直し、比較条件の緩和、畑状態の自動調整、計測グラフや保存形式の拡張。まず選択→計測→結果確認の往復を短くする。
- 性能: 既存の固定SpriteプールとラベルAtlasを再利用する。ラベル追加でAtlasサイズは1280x2504へ増える。毎フレームのGPU資源生成は追加しない。CPUの表示文字列生成と個別Sprite DrawCallは残るため、描画コスト改善まで実証したとはしない。
- 実機画面取得は `foreground window did not report a process id` で停止。ユーザー提供の16:05:24画像で、Ａ登録後のプレイ画面・Ｂ選択待ち・一時停止・数値表示を確認した。最下段が画面端に寄るため16px上へ調整。修正後の実操作と完了までの計測は未確認。矩形重複、範囲外、無効操作、各比較ステータスの表示はCPUテストで検証する。

- FarmRuntimeUI: Spriteで表示し、矩形から入力要求を得る。農業の数値を直接変更しない。
- FarmRuntimeController: メニュー状態・入力排他・確認操作を管理し、既存SystemとCommand Bridgeへ要求する。
- GamePlayScene: Release時の入口、停止判定、描画呼出しを接続する。
- GPU: フレームごとに同じSprite定数を使い回さず固定数のSpriteを初期化時に確保。242個のSpriteは既存実装のCommitted Resourceを使うため、初期GPUメモリとDrawCall増加は残る。Batch化やGPU性能改善まで検証したとは言わない。
- 実機確認: 1280×720と1920×1080でメニューの見切れとクリック位置を確認。農場全体が上下HUDの間に入るよう俯瞰カメラを調整した。売買の個数・所持金変化、高さプレビュー取消、道具欄クリックの入力分離も確認した。
- 未解決の評価項目: その他の解像度、全操作の通し録画、実キーボードによるメニュー操作、ゲームパッドだけでの操作。参考画像を見ただけで「最も使いやすい」と断定しない。

操作の詳細は[Release操作表](FarmReleaseControls.md)を参照。

## 2026-09-10 土づくり表示の調査と採用方針

- [Farming Simulator公式・Precision Farming](https://www.farming-simulator.com/newsArticle.php?campaignIndex=5&country=cl&lang=en&news_id=194): 土の窒素状態と作物の目標量を照らし合わせる設計を参考に、現在の養分・作物の目安・消費量を同じページに置く。検索結果の公式本文抜粋を参照。ページ本体は取得失敗のため、画面を実見したとは扱わない。
- [Stardew Valley Wiki・Fertilizer](https://wiki.stardewvalley.net/Fertilizer): 品質・成長速度・保水という効果の分け方と植付け前の準備を参考にする。堆肥の効果を養分補充と品質に限定し、費用や種類は増やさない。
- 既存5タブを保ち、農作業ページから土づくり詳細へ進む。現在量・目安・消費量・育成中の評価を数値で示し、使用不可時にも理由を表示する。メニューの時間停止中に判断できる。
- ImGuiは4軸レーダーに番号を付け、日本語と0～100値を併記。色だけで伝えず、狭いInspectorで軸名を重ねない。Releaseは既存SpriteとAtlasの文字表示を使用する。
- 調査からの設計判断であり、実在農業の再現やユーザビリティの優劣を実証したものではない。実画面の検証結果は作業記録に別記する。
