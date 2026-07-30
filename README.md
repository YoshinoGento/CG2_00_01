# CG4 評価課題

提出者: LE3C_26 ヨシノゲント

## 最短の確認手順

1. `01_実行ファイル/CG2_00_01.exe` を起動します。
2. TITLE画面で `Enter`、ゲームパッドの `A` / `START`、または `START GAME` を押します。
3. 画面上部の `CG4 評価課題` → `CG4評価用UI` を選びます。
4. 右側の `評価デモ・プリセット` から確認したい項目を選びます。
5. `評価項目の準備状況` が `READY` または `ACTIVE` であることを確認します。

評価操作にはDevelopment x64版を使用してください。実行中のウィンドウ最大化・サイズ変更には未対応のため、起動時のサイズのまま確認してください。

ゲーム画面では、ゲームパッドの左スティックで移動し、`A`ボタンでジャンプします。移動中はHumanのWalk Animation、ジャンプ中は空中状態が反映されます。キーボードの`WASD`と`Space`も使用できます。`F1`でPlay表示へ入ると、自動的に`ゲーム画面`プリセットとPlayer Cameraへ切り替わります。Debug表示ではGame Viewport上にマウスを置いて操作してください。`F2`でPlayer CameraとDebug Cameraを切り替えます。

## 実装した加点項目

| 評価項目 | 対象点 | 実装・確認方法 |
|---|---:|---|
| SkinningModelの表示 | 20 | `スキニング`。HumanのWalk/Sneak Meshを表示し、Animationを再生します。 |
| ComputeShaderによるSkinning | 10 | `ComputeShader Skinning` をONにします。OFFでVertexShader経路、ONでComputeShader経路へ切り替えられます。 |
| MultiMesh / MultiMaterial | 5 | `MultiMesh + MultiMaterial`。1つのModelから2 Mesh・3 Materialを表示します。 |
| Animation補間 | 5 | `Walkへ補間` / `Sneakへ補間`。移動・拡縮はLerp、回転はQuaternion Slerpで補間します。進捗率も表示します。 |
| 骨のデバッグ表示 | 10 | `骨デバッグ`。Joint階層、名前、親子関係、Local Transform、Skeleton座標、RGB軸を表示します。 |
| 手からParticleを出す | 10 | `左手 水エフェクト`。`mixamorig:LeftHand` に追従する小さな水流をGPU Particleで表示します。 |
| 武器を手に持たせる | 10 | `右手武器`。Resources内のBusterSwordを `mixamorig:RightHand` に追従させます。 |
| GPU Particle | 20 | 左手水エフェクトをComputeShaderで生成・更新します。発生位置はAnimation中の左手Jointを毎回取得します。 |

明示された加点項目の対象点合計は90点です。最終的な採点は授業の評価基準に従います。

## CG4評価用UIの操作

### スキニング・Animation

- `Model`: Animation Modelを選択します。
- `Walkへ補間` / `Sneakへ補間`: 2つのHuman Animation ClipをCross-fadeします。
- `補間時間`: 0.05～1.00秒で変更できます。
- `Show Model`: Mesh表示を切り替えます。
- `Show Skeleton`: Skeleton表示を切り替えます。
- `ComputeShader Skinning`: VertexShader / ComputeShaderのSkinning経路を切り替えます。
- `Play Animation`, `Reset Time`, `Timeline`, `Playback Speed`: 再生状態を操作します。

### 骨デバッグ

- Rootは緑、通常Jointは水色、選択チェーンはマゼンタで表示します。
- 選択Boneは親Jointから選択Jointまでを黄色い立体枠で表示します。
- `選択Joint RGB軸`, `全Joint RGB軸`, `Jointマーカー`, `選択チェーン強調` を切り替えられます。
- 名前付きJoint階層をクリックして選択できます。
- 選択JointのIndex、Parent、Children、Local Position、Quaternion、Scale、Skeleton Positionを確認できます。

### BusterSword

- `BusterSwordを表示`: 武器表示を切り替えます。
- `武器Socket Gizmo`: RightHandの追従位置と武器Boundsを表示します。
- `武器 Local Position / Rotation / Uniform Scale`: Bone Local座標で装着位置を調整します。
- `武器位置をリセット`: 既定位置へ戻します。

### 左手GPU Particle

- `Particle放出位置Gizmo`: LeftHand Socketの位置とRGB軸を表示します。
- `Particle Local Position`: Bone Local座標で放出位置を調整します。
- `左手から小さい水流を放出`: 水流を再発生させます。

### UI表示モード

画面上部で `CG4評価用UI` と `従来デバッグUI` を切り替えられます。既存のImGui機能は削除していません。

- `F1`: Debug表示（CG4 ImGui / Game Viewport）とPlay表示（ゲーム画面のみ）を切り替えます。
- `F2`: Player CameraとDebug Cameraを切り替えます。

## 実装上の要点

- SkinClusterはObjectごとに所有し、異なるModel間で共有しません。
- MultiMeshのBone WeightはMeshごとのVertex Baseを加算して全体Vertex Indexへ変換します。
- ComputeShader Skinning結果を保存し、通常描画・Shadow・複数描画で再利用できる構造です。
- Joint Attachmentは追従元Object、Joint名、Local Transformを分離し、武器とParticle Socketの両方で利用します。
- Bone debugは既存の`LineDrawer`へBatchし、個別DrawCallを追加していません。
- GPU Particleは既存のUAV、Free-list、ResourceBarrier/Fence管理を利用します。

## ビルド方法

- Solution: `02_プロジェクト/CG2_00_01/project/CG2_00_01.sln`
- Platform: `x64`
- 推奨Configuration: `Development`
- Visual StudioでSolutionを開き、Solution全体をBuildしてください。

提出前の最終確認ではDebug / Development / Release x64の全Rebuildに成功しています。`EmitParticle.CS.hlsl` と `Skinning.CS.hlsl` はDXC `cs_6_0` で単体コンパイル確認済みです。

## 既知の制限

- 実行中のSwapChain再生成を実装していないため、ウィンドウの最大化・サイズ変更は行わないでください。
- CG4評価用ImGuiはDevelopment / Debug構成で利用できます。Release構成にはImGui評価UIを含めていません。
