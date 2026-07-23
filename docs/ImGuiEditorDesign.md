# 農業デモ向け ImGui Editor 設計方針

## 1. この資料の目的

この資料は、現在のDirectX 12自作EngineにあるImGui Debug UIを、農業デモを制作・調整しやすいEditorへ段階的に整理するための設計資料です。

対象は次の2点です。

1. Unity、Unreal Engine、Blenderのどの考え方を参考にするか
2. 日本語を含むEditor UIを、既存のRuntimeとDirectX 12管理を壊さず実装する方法

農業Game本体、PostEffect、Particle、Field処理をこの資料だけで作り直すことは目的にしません。

---

## Unity-style Console Window

The Farm editor includes a dockable `ConsoleWindow` for engine and application diagnostics.

- `Logger` owns severity (`Info`, `Warning`, `Error`), ordering, duplicate collapse, and a thread-safe 1024-entry ring buffer.
- Existing `Logger::Log` calls remain compatible; new code should prefer explicit typed logging.
- `ConsoleWindow` owns only presentation state: severity filters, search, selection, and auto-scroll.
- `EditorShell` owns visibility, bottom Dock placement, and routes the Clear request to `Logger`.
- The UI copies a log snapshot only when the Logger revision changes. It performs no per-frame filesystem access or GPU allocation.
- Consecutive duplicate messages are collapsed and shown with a repeat count.
- Technical log messages remain English; Console labels follow the Japanese/English editor language setting.
- The window uses stable IDs (`Console###Console`) so language changes preserve Dock layout.

The Console does not allocate descriptors, create DX12 resources, issue draw calls outside ImGui, or alter ResourceState/Fence behavior.

---

## Unity-style Simulation Controls

The fixed Farm Toolbar exposes compact Play (`>`), Pause (`||`), and Step (`>|`) controls.

- `FrameClock` is the only owner of pause state, simulation delta, fixed-step accumulation, and single-step requests.
- `FarmToolbarWindow` reads immutable state and emits a typed `SimulationEditorAction`; it does not mutate runtime state.
- `EditorShell` routes the action through `GamePlayEditorBridge`, which validates the bound Scene and forwards it to `FrameClock`.
- Pause sets simulation delta to zero and consumes no fixed steps. Step is enabled only while paused and advances one fixed step plus one fixed-duration variable update before returning to Pause.
- Farm growth, gameplay, object animation, CPU/GPU particles, and PostEffect time use simulation delta. Debug-camera navigation uses real delta so inspection remains possible while paused.
- GPU Particle simulation Dispatch is skipped while paused, but the resource still transitions to `NON_PIXEL_SHADER_RESOURCE` before drawing.

The controls add no pause flag to `Game`, `Scene`, or UI classes and create no descriptors, PSOs, GPU resources, or additional draw calls.

---

## 2. 結論

### 採用する基本方針

- Editor全体の構成は **Unity型** を主軸にする
- Property編集は **Unreal EngineのDetails Panel** を参考にする
- 作業内容の切り替えは **BlenderのWorkspace** を参考にする
- 農業デモ固有の操作として **Farm Overview** を追加する

3つのEditorをそのまま複製するのではなく、現在のEngine規模に必要な部分だけを採用します。

| 参考Editor | 採用する要素 | 採用しない要素 |
|---|---|---|
| Unity | Hierarchy、Scene/Game View、Inspector、Console、Play制御 | PrefabやPackage Manager相当の大規模機能 |
| Unreal Engine | Details Panelのカテゴリ分け、Toolbar、検索しやすいProperty表示 | Blueprint、Content Browser全体、複雑なAsset Pipeline |
| Blender | Farm / Effects / Rendering / DebugのWorkspace切り替え | 自由度が高すぎるArea分割と複雑な操作体系 |

### このEngine向けの重要な追加判断

一般的なHierarchyだけでは、3x3の畑を素早く選択・比較する用途に向きません。
そのため、畑を上から見たGridとして直接選べる `Farm Overview` をEditorの主要Windowにします。

---

## 3. 現在の状態

現在のEngineには、農業デモを調整するための土台がすでにあります。

- DirectX 12描画基盤
- Dear ImGui 1.92.6
- Docking / Multi-Viewport
- Game Viewport
- FieldManagerと畑操作
- GPU Particle / Particle Interaction
- Harvest Impact / Digital Impact / Crop Burst
- PostEffectChain
- Skybox切り替え
- Debug Windowの一部分離
- Release / Presentation表示

一方、Editorとしては次の問題が残っています。

- `Game.cpp` がDockSpaceと多数のDebug Window生成を直接担当している
- UIから`GamePlayScene`の公開状態を直接変更する箇所がある
- 選択状態がWindowごとに分散しやすい
- Debug Windowが増えるほど、目的の設定を探しにくくなる
- Console、編集履歴、Play/Pause/Stepなど制作Editorの基礎機能が不足している
- Farm、Effect、Renderingの操作が同じ画面に混在している

このままWindowを追加し続けると、`Game`と`GamePlayScene`がさらに巨大化します。
先にEditor Shellと操作要求の境界を作る必要があります。

### 2026-07-21 Editor基盤進捗

- `EditorShell`を追加し、DockSpaceと既存Editor Windowの所有・描画を`Game`から移動した
- `GameViewportWindow`を追加し、`FinalDisplayTexture`表示、Aspect維持、Viewport矩形、Mouse座標補正を分離した
- `Game`はImGui frame中に`EditorShell::Draw()`を呼ぶ構造へ縮小した
- Window名、Scene更新順、PostEffect実行順は維持している
- `EditorSelectionSystem`でFarm Tileの選択indexとFarmGrid generationを管理する
- `FarmOverviewWindow`から3x3畑の選択、Tool適用、高さ変更、Undo / Redoを実行できる
- Camera、Visibility、Object、Particleをread-only ViewDataとtyped Commandへ移行した
- `GamePlayEditorBridge`だけがScene/SystemへのEditor操作境界となり、各Windowの`friend`依存を廃止した

---

## 4. 目標画面構成

最初の標準Layoutは次の構成にします。

```text
┌ Menu / Play / Pause / Step / Workspace ────────────────┐
├ Farm Overview ┬ Scene View / Game View ┬ Inspector ────┤
│ 3x3 Field     │                        │ Tile          │
│ Crop List     │                        │ Crop          │
│ Tool Palette  │                        │ Growth        │
├───────────────┴────────────────────────┴───────────────┤
│ Console / History / Assets / Performance               │
└─────────────────────────────────────────────────────────┘
```

### 左: Farm Overview / Hierarchy

- 3x3畑をGrid表示する
- FieldStateを色とIconで表示する
- 選択中Tileを明確に表示する
- Empty / Tilled / Watered / Growing / Readyを比較できる
- 作物、Effect発生位置、Camera targetを選択できる

### 中央: Scene View / Game View

- `Scene View`: Editor用Cameraで配置やEffect位置を確認する
- `Game View`: `FinalDisplayTexture`を表示し、実際のGame出力を確認する
- 将来的にGizmoを追加しても、Game ViewのRuntime入力とは分離する

### 右: Inspector

- 選択中Tile、Crop、Emitter、PostEffect PassのPropertyを表示する
- Propertyは `Transform`、`Field`、`Crop`、`Visual`、`Effect` のように分類する
- 検索FilterとReset操作を用意する
- UIは値を直接書き換えず、Systemへ操作要求を通知する

### 下: Console / History / Assets / Performance

- Error、Warning、InfoをFilterできるConsole
- Undo / Redo対象の操作履歴
- 農業Demoで使うTexture、Model、Effect設定の一覧
- FPS、Frame time、Particle数、Descriptor使用数などのStatus

---

## 5. Workspace構成

### Farm Workspace

主な用途は畑と作物の編集です。

- Farm Overview
- Scene View / Game View
- Field Inspector
- Crop Inspector
- Farm Tool Palette
- History

### Effects Workspace

主な用途は収穫演出とParticleの調整です。

- Effect一覧
- Preview Viewport
- Emitter / Particle Parameter Inspector
- Harvest / Digital / Crop Burstの再生Button
- Particle alive count、Dispatch数、Buffer使用量
- FieldFXとAccentFXの表示状態

### Rendering Workspace

主な用途はPostEffectと最終出力の確認です。

- PostEffectChainの有効状態と実行順
- Ping / Pong / PostEffectResult / FinalDisplayTextureのPreview
- HSV、Vignette、RadialBlur、RandomNoiseなどのParameter
- Skybox / Lighting
- Gamma Correctionが最後に1回だけであることの表示

### Debug Workspace

主な用途はEngine内部状態の調査です。

- Input状態
- Animation / Skinning
- Visibility / Culling
- Camera値
- DX12 ResourceState
- SRV / RTV / UAV descriptor使用状況
- GPU ParticleのDispatch数とalive count
- Frame time / DrawCall / GPU待ちの確認

---

## 6. 必要な責務分担

採用作品として保守できる構造にするため、次の境界を守ります。

| Class / System | 責務 |
|---|---|
| `Game` | 起動、Main Loop、Scene切り替えだけを担当する |
| `EditorShell` | DockSpace、Menu、Toolbar、Workspace、Window表示状態を管理する |
| `GameViewportWindow` | `FinalDisplayTexture`表示とViewport矩形を提供する |
| `SceneViewportWindow` | Editor CameraによるScene確認とGizmo入力を担当する |
| `FarmOverviewWindow` | 畑の一覧表示と選択要求を通知する |
| `FarmInspectorWindow` | 選択対象を表示し、編集要求を通知する |
| `EditorSelectionSystem` | 現在選択中のTile / Object / Effectを一元管理する |
| `FarmToolActionSystem` | Till / Water / Plant / Harvestなどの状態変更を実行する |
| `EditorCommandHistory` | Undo / Redo可能な変更履歴を管理する |
| `EditorConsole` | Logを蓄積し、LevelやCategoryでFilterする |
| `EditorSettings` | Layout、UI scale、Window表示、Workspaceを保存する |
| `ImGuiManager` | ImGui Context、Backend、Font Atlas、ImGui用descriptorを管理する |

### UIの禁止事項

Editor Window内で次の処理を直接行いません。

- FieldStateの直接変更
- Scene objectの所有権変更
- Particle bufferの生成・破棄
- DX12 ResourceBarrierの発行
- Gameplay timerの進行

UIは「何を変更したいか」を通知し、実際の変更は対応するSystemが行います。

```text
User Input
    ↓
Editor Window
    ↓ Action Request
System / Command
    ↓
Runtime State
    ↓
Editor Windowへ結果を表示
```

---

## 7. 最初に不足している機能

優先度順に実装します。

### Priority 1: Editorの土台

- `EditorShell`の分離
- `GameViewportWindow`の分離
- `EditorSelectionSystem`
- Play / Pause / Step
- Console
- Layout Reset
- UI Scale

### Priority 2: 農業制作機能

- Farm Overview
- Field / Crop Inspector
- FieldStateごとのFilter
- Till / Water / Plant / Harvestの操作要求
- Undo / Redo
- JSON設定のDirty表示と保存

### Priority 3: Effect制作機能

- Effect Preview
- Particle Parameter Preset
- FieldFX / AccentFXの切り替え
- Crop BurstのPhase表示
- Particle数、Dispatch数、Buffer状態の可視化

### Priority 4: Rendering確認

- PostEffectChain順序表示
- RenderTexture Preview
- ResourceState表示
- Descriptor使用数表示
- GPU timing

---

## 8. 実装順

### Phase 1: 現在の見た目を変えずにShellを分離

1. `Game.cpp`からDockSpace生成を`EditorShell`へ移す
2. Game Viewport表示を`GameViewportWindow`へ移す
3. 既存Debug Windowの表示ON/OFFを`EditorShell`へ集約する
4. Runtimeの描画順と入力結果が変わらないことを確認する

### Phase 2: 選択と編集の境界を作る

1. `EditorSelectionSystem`を追加する（完了）
2. UIから`GamePlayScene`内部状態への直接書き込みをやめる（完了）
3. Farm操作を`FarmToolActionSystem`またはCommand経由にする（完了）
4. ConsoleとHistoryを追加する

### Phase 3: Farm Workspaceを完成させる

1. Farm Overviewを追加する（基本操作まで完了）
2. Field / Crop Inspectorを追加する
3. 状態色、Icon、Tooltipを統一する
4. Undo / Redoと保存状態を追加する

### Phase 4: Effects / Rendering Workspaceを追加

1. Effect PreviewとPresetを追加する
2. PostEffectChainの順序とParameterを表示する
3. RenderTextureとParticle statisticsを可視化する

### Phase 5: Editor品質を上げる

1. Layout保存とReset
2. DPI / UI Scale対応
3. Keyboard navigation
4. Multi-ViewportのDX12動作を再検証した後、必要な場合だけ有効活用する

---

## 9. ImGui日本語Font対応

### 実装済み内容

- Project同梱Font: `project/Resources/fonts/NotoSansJP-VF.ttf`
- License: `project/Resources/fonts/OFL.txt`
- Font初期化: `project/engine/base/ImGuiManager.cpp`
- 既定Font size: 18px
- 選択UI: Main Menu Barの `表示 > フォント`

起動時は次の候補を一度だけ登録します。

1. Project同梱のNoto Sans JP。存在しない場合はWindowsのNoto Sans JP
2. Windowsに存在する場合だけYu Gothic
3. Windowsに存在する場合だけMeiryo
4. すべて失敗した場合だけImGui内蔵Font

Fontが存在しない場合でも起動失敗にはせず、Loggerへfallbackを記録します。

Dear ImGui 1.92.6のdynamic glyph loadingを利用するため、全CJK glyphを起動時に事前生成しません。これにより、Font Atlasの過剰なVRAM消費と長い初期化時間を避けます。

Font切り替え時はAtlasを再構築せず、`ImGuiIO::FontDefault`を登録済みFontへ変更します。
そのため、切り替えごとのGPU待ち、Font Texture再作成、SRV descriptor再割当は発生しません。

### 日本語文字列のルール

- SourceとJSONはUTF-8を使用する
- 表示文字列をShift-JISへ変換しない
- Low-level API名、DX12用語、Class名は英語を維持する
- UI Labelは短くし、Tooltipで補足する
- 文字化け時はFontより先にSource file encodingを確認する

### 日本語 / Englishの使い分け（実装済み）

Editorは日本語優先ですが、すべてを翻訳しません。Unityなどの日本語UIで起きやすい「資料やAPI名を検索しにくい」「Source上の識別子と画面名が一致しない」という問題を避けるため、次の境界にしています。

| 対象 | 表示方針 | 例 |
|---|---|---|
| Farm制作の操作、状態、確認、エラー | 日本語 | 耕す、水やり、操作履歴、未保存、削除確認 |
| Editorの一般操作 | 日本語 | 表示、ウィンドウ、フォント、レイアウト初期化 |
| ショートカット | Key名は英語、説明は日本語 | `Ctrl+S: 変更を保存` |
| DirectX 12 / GPU / Shader / PostEffect | 英語 | DescriptorHeap、ResourceBarrier、SRV、UAV、HSVFilter |
| Class、enum、JSON key、save ID、file path | 英語の内部識別子を維持 | `FarmDocumentSystem`、`displayName` |
| ユーザーが付けた名前 | 翻訳しない | 畑のセーブ名、作物名 |

主要なFarm UI文字列は `EditorLocalization.h/.cpp` の静的Catalogへ集約しています。Windowタイトルは表示名の後ろに固定English IDを付けます。

```text
畑マップ###FarmMap
Farm Map###FarmMap
```

言語を切り替えてもImGui IDは`FarmMap`のままなので、Docking、Window開閉状態、保存済みLayoutが別Windowとして分裂しません。未知の文字列はEnglishへfallbackし、翻訳漏れで空Labelやnullptrを渡しません。

Main Menuの `表示 > 表示言語`、またはEditor Settingsの `表示言語` から次を選択できます。

- 日本語
- English

選択値は `Settings/editor/editor_settings.json` の `language` へ保存し、次回起動時に復元します。日本語とEnglishの切り替えではFont Atlas、SRV descriptor、GPU resourceを再作成しません。

### Editor Settings（実装済み）

`Editor Settings` Windowから次を変更できます。

- 表示言語: 日本語 / English
- 登録済みFontの選択
- UI Scale: 75%から150%
- Theme: Dark / Light / Classic
- Window LayoutのReset

表示言語、Font、UI Scale、Themeは `Settings/editor/editor_settings.json` へ保存し、次回起動時に復元します。
設定変更中はメモリ上だけを更新し、通常終了時に一度だけ保存するため、Slider操作中に毎Frame JSONを書きません。

UI Scale適用時は既定Styleから再構築してからScaleするため、変更を繰り返してもPaddingやSpacingが累積拡大しません。
Font切り替えは従来どおり登録済みFontの参照変更だけで、Font Atlas、SRV descriptor、GPU resourceを再作成しません。

### Editor Window分割（実装済み）

旧 `DrawLegacyGamePlayWindows()` は廃止し、次へ分割しました。

- `VisibilityWindow`
- `CameraControlWindow`
- `ObjectInspectorWindow`
- `ParticleEffectWindow`
- `EditorSettingsWindow`

`EditorShell`はDockSpace、Window表示順、Window表示切り替えだけを統括します。
上記4つのWindowは `GamePlayEditorViewModel` 内のread-only ViewDataを描画し、typed Commandだけを返します。
`EditorShell`はCommandを `GamePlayEditorBridge` へ転送し、Bridgeが入力範囲、nullptr、Animation/Joint indexを検証してからScene/Systemへ適用します。

### Selection / Command境界（実装済み）

- `EditorSelectionSystem`: 選択種別、index、FarmGrid generationを保持する
- `GamePlayEditorViewModel`: Farm、Visibility、Camera、Object、Particleの表示用Snapshotを保持する
- `FarmOverviewWindow`: 3x3 Gridと選択Tile詳細を表示し、操作要求だけを返す
- `GamePlayEditorBridge`: 非所有参照をBindし、Farm/System操作とScene Editor設定を検証して適用する
- `FarmToolActionSystem`: Farm変更とUndo / Redoを担当する

Farm選択CommandはFarmGrid generationが一致しない場合に拒否します。Grid再生成後の古い選択indexを適用しません。
Camera速度、Light、Primitive、Particle設定はBridgeで範囲を制限し、Animation Object、Skeleton、Joint、ParticleManagerは使用前に存在確認します。
Editor操作で新しいDX12 descriptor、RenderTexture、PSOは作成せず、既存の描画ResourceStateにも介入しません。

将来Font Atlas再構築が必要になった場合は、GPUが参照中のFont Textureを上書きしません。

---

## 10. DirectX 12で守ること

ImGui Editorを増やしても、DX12 Resource管理を曖昧にしてはいけません。

### Descriptor

- ImGui backendの要求ごとに固有のSRV descriptorを割り当てる
- 同じdescriptorをFont、Viewport、Texture Previewで使い回さない
- 解放callbackで`SrvManager`へ返却する
- Descriptor Heap上限を超える前に使用数を可視化する

現在の`ImGuiManager`は、ImGui backendのallocate/free callbackを`SrvManager`へ接続し、CPU handleとSRV indexの対応を追跡しています。

### Resource lifetime

- Viewportに表示中のTextureをFrame途中で破棄しない
- GPUが参照中のFont TextureやPreview Textureを上書きしない
- Multi-Viewportを使う場合は、各WindowのSwapchainとFrame resourceの寿命を確認する

### ResourceState

- `FinalDisplayTexture`は表示時に`PIXEL_SHADER_RESOURCE`であること
- Texture Preview追加時も、同じResourceをSRV入力とRTV出力へ同時Bindしない
- UAV書込み後は用途に応じてState transitionとUAV barrierを使い分ける
- before / afterが同じBarrierを発行しない

### Performance

- Editor Windowを開くたびにGPU resourceを作り直さない
- 毎Frameの文字列生成とdynamic allocationを抑える
- Particle全要素をCPUで走査しない
- Texture Preview数と追加DrawCallを計測する

---

## 11. 初期段階で採用しないもの

次の機能は規模が大きく、農業デモ制作の優先度が低いため後回しにします。

- Blueprint相当のVisual Scripting
- Prefab制作System
- Full Asset Import Pipeline
- Blender型の自由なWindow分割Editor
- RenderGraph Editor
- Node-based PostEffect Editor
- 複数Sceneの同時編集
- Editor用Plugin System

必要になった時点で、現在のSystem境界を壊さず追加できるか再評価します。

---

## 12. 現在の確認結果

### 確認済み

- 日本語Font assetをProjectへ同梱
- Debug x64: 警告0 / エラー0
- Release x64: 警告0 / エラー0
- Debug実行で初期化直後のassert・異常終了なし
- ImGui用SRV descriptorの単一index使い回しを廃止
- 日本語 / English切り替えと再起動後の復元
- 言語切り替え後も固定`###` IDでDock配置を維持
- Noto Sans JP / Meiryoによる日本語glyph表示

### 未確認

- 125% / 150% DPIでの文字サイズとWindow配置
- Multi-Viewportを長時間使用した場合のdescriptor寿命
- Editor Shell分離後の操作性

---

## 13. 完成判定

最初のEditor改善は、次を満たした時点で一区切りとします。

- `Game`がEditor Windowの詳細を知らない
- Farm Overviewから畑を直接選択できる
- Inspectorが状態を直接変更せずSystemへ要求を送る
- Scene ViewとGame Viewを区別できる
- Play / Pause / Stepが動作する
- ConsoleでErrorとWarningを確認できる
- 日本語LabelがDebug / Releaseで表示できる
- LayoutとUI Scaleを復元できる
- DX12 descriptor、ResourceState、GPU lifetimeに違反がない

この段階を先に完成させてから、Effects Workspaceや高度なRendering Debugへ広げます。

---

## 14. Farm Workspace v1 実装状況

2026-07-21時点で、農業デモの操作確認に必要な最初の標準Layoutを実装しました。

```text
Main Menu
+------------------------------------------------------------+
| Farm Toolbar: Tool / Undo / Redo / Selection / Layout Reset|
+-------------+----------------------------+-----------------+
| Hierarchy   | Game Viewport              | Farm Inspector  |
| Farm / Row  | FinalDisplayTexture        | Tile / Tool     |
| / Tile      | Mouse select / quick apply | Apply status    |
+-------------+----------------------------+-----------------+
| Farm History: Undo / Redo count and next command            |
+------------------------------------------------------------+
```

### 実装済み

- 初回または明示的なReset時だけDockBuilderで標準Layoutを構築する
- 2回目以降はImGui iniのユーザー配置を尊重する
- `Farm Hierarchy`からTile選択Commandを送る
- `Farm Inspector`からTool選択と適用Commandを送る
- `Farm Toolbar`からTool選択、Undo、Redo、Layout Resetを行う
- `Farm History`でUndo / Redo件数と次のCommand名を確認する
- Editor WindowはFarm状態を直接変更せず、`GamePlayEditorBridge`経由でSystemへ通知する

### 未実装

- RuntimeのPlay / Pause / Step制御
- Error / Warningを集約するConsole
- Asset Browser
- Effects / Rendering / Debug Workspace切り替え

未実装機能はToolbarへ仮ボタンとして追加していません。Runtime APIと責務境界を設計してから追加します。
