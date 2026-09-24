# 畑の作物外観の検証 2026-09-22

## 変更範囲

- FarmMeshLayout: 根の色、葉の色と種別の選択のみ。成熟時の2パーツ、ニンジン84%／カブ50%の根の埋まりと葉の接続位置を維持。
- FarmRenderer: ModelManager所有の共有葉モデルを2種類へ。既存モデルがそろわなければ従来のwire表示に委ねる。追加のフレーム内ロード、描画パーツ、保存データはない。
- 原形生成: generate_farm_crop_meshes.py に閉じた葉のリボン形状を追加。新規 crop_turnip_leaves.obj / crop_carrot_leaves.obj。従来の根と汎用葉OBJは変更なし。
- Object3d::SpecularType::None = 2 と Object3D.PS.hlsl の分岐を追加。既存Material整数枠を使用し、レイアウト、RootSignature、Descriptor、Barrier、Fenceは変更しない。既定値BlinnPhongは維持。FarmRendererは再利用Objectの非作物材質を毎回BlinnPhongに戻す。
- Game／Scene／UIへ処理を追加せず、成長・品質・売買・セーブの責務は変更なし。

## 自動検証

- `tools/tests/farm_crop_mesh_asset_test.py`: 合格。有限値、正の体積、閉じた向き付き辺、非退化三角形、y=0..1／水平半径1以内、再生成一致、実OBJの丸め後の形状を確認。カブ340／ニンジン460三角形。
- `tools/tests/run_farm_crop_visual_size_test.cmd`: 合格。芽から成熟までの葉の種別、サイズ制限、斜面中心への植付け、根の埋まり、収穫時の引抜き／保持／縮小、停止、OOB、世代不一致を確認。
- 最終 Debug／Release x64 ビルド成功。追加中に列挙子等が重複する編集エラーを修正し、最終再ビルドを実施。

## Release 実画面

`generated/codex_checks/field_art_20260922` の専用Settingsと合成農場を使用。元のSettingsや提出動画は変更せず、週次報告の成果証拠として流用しない。

- 1280x720: 5%／40%／80%／100%の作物と高さ0／1で、芽・葉・根を表示。カブの広い葉とニンジンの切れ込みを確認。
- 初回の最大化確認で葉に白い鏡面反射を認め、作物限定のNone材質を追加。最終Releaseの見下ろし／追従カメラで反射の除去、緑の葉、根の接地を確認。他のプレイヤーモデルは既存の光沢を維持。
- 初回最大化操作の後、時間倍率が4xへ変化したため停止した。この入力現象の原因調査・修正は未実施。追従カメラでは既存HUDと農場上端が重なる配置も残る。
- 所有する検証ゲームは通常終了。今回、最終材質での最大化再確認、収穫アニメーションの実画面再実行、Debug実画面の確認はしていない。

## 性能と残件

葉の三角形は従来共通40から340／460へ増加するが共有モデルで、作物あたりのDrawCallは従来どおり最大2、Shadowも同じパーツ数。透明材質／Texture／Descriptor追加はない。実際のVRAM・GPU時間・大量配置時の計測は未実施。最終HD-2D Sprite、全背景アート、自由視点すべての見え方は未完成。
