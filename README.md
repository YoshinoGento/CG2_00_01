[![DebugBuild](https://github.com/YoshinoGento/CG2_00_01/actions/workflows/DebugBuild.yml/badge.svg)](https://github.com/YoshinoGento/CG2_00_01/actions/workflows/DebugBuild.yml)
[![ReleaseBuild](https://github.com/YoshinoGento/CG2_00_01/actions/workflows/ReleaseBuild.yml/badge.svg)](https://github.com/YoshinoGento/CG2_00_01/actions/workflows/ReleaseBuild.yml)
[![DevelopmentBuild](https://github.com/YoshinoGento/CG2_00_01/actions/workflows/DevelopmentBuild.yml/badge.svg)](https://github.com/YoshinoGento/CG2_00_01/actions/workflows/DevelopmentBuild.yml)

# 加点項目

* ローダーと配置
  * `Resources/levels/scene.json` を `nlohmann/json.hpp` で読み込み、`LevelData` / `ObjectData` / `TransformData` に格納しています。
  * Blender の座標系差を吸収し、`MESH` の `file_name` をもとに `Object3d` を生成して配置します。
  * `disabled` が true の MESH は生成対象から除外します。
* コライダーをゲーム側当たり判定に適用
  * `collider` の `type` / `center` / `size` を読み込み、ゲーム画面上で青い wire box として反映確認できます。
* ホットリロード
  * GamePlayScene の debug window の `Reload Level (R)` ボタン、または `R` キーで `scene.json` を再読み込みできます。
* 無効フラグの追加
  * `disabled` を JSON から読み込み、赤い wire box で確認できます。
* SpawnPoint の配置
  * `SPAWN_POINT` / `spawn_type` / `enemy_type` を読み込み、緑の marker として表示します。
* イベントトリガーの配置
  * `EVENT_TRIGGER` / `event_id` / `collider` を読み込み、橙色の trigger box として表示します。
* ギミックの配置
  * `GIMMICK` / `gimmick_type` / `required_key_id` / `collider` を読み込み、紫色の gimmick box として表示します。
* カメラの配置
  * `CAMERA` の transform を読み込み、GamePlayScene の初期カメラ位置へ反映します。
* 敵の巡回ルートを配置
  * `PATROL_POINT` を順番に読み込み、シアンのルート線と黄色の移動 marker として表示します。

# 独自実装

* JSON 任意項目の型検証
  * 欠落してよい項目と必須項目を分け、型が不正な場合はログを出してロード失敗にします。
* 評価動画用の debug gizmo 表示
  * 読み込んだ collider、spawn、trigger、gimmick、camera、light、patrol route、disabled を色分けして表示します。
* DirectX12 描画基盤を変更しない統合
  * Barrier / Fence / Descriptor / VRAM 管理、既存 pipeline は変更せず、既存 `Object3d` と `LineDrawer` に限定して統合しています。
