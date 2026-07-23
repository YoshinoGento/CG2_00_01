# CG5評価課題 補足ドキュメント

## 提出者情報

| 項目 | 内容 |
|---|---|
| クラス | `[記入してください]` |
| 出席番号 | `[記入してください]` |
| 氏名 | `[記入してください]` |
| 作品名 | Farm PostEffect Demo |

> このファイルは作業中の `Read.md` です。配布された提出方法では補足資料名が
> `ReadMe.md` と記載されているため、提出直前に指定されたファイル名へ合わせてください。

## 作品概要

農業ゲームの操作画面を使い、必須課題の Grayscale と複数の PostEffect を確認できるデモです。
各 PostEffect は別画面の画像加工サンプルではなく、畑・作物・Particle・HUDを含む実際の
GamePlay Sceneへ適用しています。

Debug / Developmentビルドでは、`Window > Fullscreen PostEffect` から同じCamera位置のまま
Original Colorと各Effectを切り替えられます。画面下部の `PostEffect Workspace` には、
評価項目に対応したQuick Checkボタンと調整用Parameterを用意しています。

## 実装済みの主な機能

- 必須課題のGrayscaleを実際のFarm GamePlay Sceneへ適用
- Vignette、BoxFilter、GaussianFilter、Outline、RadialBlur、Dissolve、
  RandomNoiseなど、複数種類のPostEffectを実装
- `Chain Mode OFF`では、選択したPostEffectを1種類だけ実行
- `Chain Mode ON`では、有効にした複数のPostEffectを順番に重ね掛け
- 複数Effectの中間結果はPing/Pong RenderTextureへ交互に出力し、
  同じTextureを入力SRVと出力RTVへ同時指定しない構成
- EffectごとにON/OFFとParameter調整が可能
- 有効なEffectが0個の場合はCopy Passへフォールバック
- Linear空間でPostEffectを処理し、最後の`LinearToSRGB`でGammaCorrectionを1回だけ実行
- `FinalDisplayTexture`をSwapchainとGame Viewportの共通表示元として使用
- Release提出デモでは、各Effectの違いを比較しやすくするため、Spaceで1種類ずつ切り替えて表示

## 提出物チェック

配布資料に従い、次の3点を1つの提出フォルダへ入れます。

- 実行ファイル一式
- ビルド可能なProject Folder一式
- 補足ドキュメント（このファイル）

提出フォルダ名:

```text
クラス_出席番号_フルネーム
```

ZIP名の例:

```text
LE3A_99_カマタ_タロウ.zip
```

提出前に、展開したZIPだけを使って別の場所から起動・ビルドできることを確認してください。

## 動作環境

- Windows 10 / 11
- DirectX 12対応GPU
- x64
- Visual StudioとMSVC Platform Toolset `v145`
- Windows SDK

使用している主なLibrary:

- DirectX 12
- DirectX Shader Compiler（DXC）
- DirectXTex
- Dear ImGui
- Assimp
- nlohmann/json

Fontや外部LibraryのLicenseは、Project内に同梱されている各Licenseファイルを参照してください。

## Build方法

1. `project/CG2_00_01.sln` をVisual Studioで開きます。
2. Platformを `x64` にします。
3. 用途に応じてConfigurationを選びます。
4. SolutionをBuildします。

| Configuration | 用途 | ImGui Editor |
|---|---|---|
| Development x64 | Parameter調整とSource確認に推奨 | あり |
| Debug x64 | Debug Layerを含む開発確認 | あり |
| Release x64 | 提出用。GamePlayへ直行し、SpaceでEffect切り替え | なし |

主な出力先:

```text
generated/outputs/Development/CG2_00_01.exe
generated/outputs/Debug/CG2_00_01.exe
generated/outputs/Release/CG2_00_01.exe
```

ShaderやTextureは `Resources/...` の相対Pathで読み込むため、実行時のCurrent Directoryは
`project` にしてください。実行ファイル一式を別Folderへまとめる場合は、`Resources`と
必要なDLLを含め、相対Pathが成立する配置にしてください。

PowerShellからDevelopment版を起動する例:

```powershell
Set-Location project
..\generated\outputs\Development\CG2_00_01.exe
```

## 起動とPostEffect確認

1. Development / Debug版では、Title Sceneで `Enter` を押してGamePlay Sceneへ移動します。
2. `Window > Fullscreen PostEffect` を開きます。
3. `Original Color` で元画面を確認します。
4. `Required Grayscale` で必須課題を確認します。
5. `Scored Effects` の各Buttonで加点候補を確認します。
6. 選択中EffectのParameterを変更し、処理結果が連続的に変化することを確認します。

Quick Checkを選ぶとChain ModeをOFFにし、比較用の一定Parameterを設定します。
これにより、別Effectによる上書きを避けて同じSceneを比較できます。

Release版はTitle Sceneを省略してGamePlay Sceneから開始します。`Space`を押すたびに、
次の提出用Presetへ切り替わります。

```text
Original Color
-> Grayscale
-> Vignette
-> BoxFilter 3x3
-> GaussianFilter 5x5
-> Luminance Outline
-> Depth Outline
-> RadialBlur
-> Dissolve
-> RandomNoise
-> Original Color
```

ReleaseのGame画面内は、現在のEffect名だけを大きく表示します。Farm HUD、操作説明、
Stage Clear HUDは提出デモでは非表示です。切り替え方法だけは、同じパネル右下へ小さく
`SPACE : NEXT`と表示します。`Dissolve`を選ぶと、同じEffect名の横に進行率を表示し、
Thresholdが4秒かけて`0% -> 100%`へ自動再生されます。Effect名はPostEffect後の表示用UIとして
描画するため、Dissolve終盤でも消えません。Window titleにも現在のEffect名を表示します。
提出用Presetでは、Dissolveを太い橙色の発光境界、RandomNoiseを大きく動く粒状ノイズに設定し、
最後の2つが「消去」と「画面全体のノイズ」として見分けられるようにしています。
Release用ControllerがSpaceのTriggerを消費するため、同じFrameで既存のParticle再生や
Player jumpは実行されません。Debug / Developmentではこの提出用Controllerを動かさず、
既存のSpace操作を維持します。

## 必須課題: Grayscale

`Grayscale.PS.hlsl` でScene colorを読み、次の輝度係数でRGBを1つの明度へ変換します。

```hlsl
float value = dot(color.rgb, float3(0.2125f, 0.7154f, 0.0721f));
```

`Grayscale Amount` によりOriginal Colorと完全なGrayscaleを補間できます。

利用方法:

- Farm GamePlay Scene全体へ適用
- `Original Color` と `Required Grayscale` を同じCameraで比較
- 畑、作物、Particle、HUDを含む最終画面で効果を確認

主要ファイル:

- `project/Resources/shader/Grayscale.PS.hlsl`
- `project/engine/effect/PostEffectSystem.cpp`
- `project/engine/effect/PostEffectManager.cpp`
- `project/engine/debug/PostEffectDebugWindow.cpp`

## 加点項目として確認できるPostEffect

下表は配布資料の項目名と、現在の実装・利用方法の対応です。

| 評価項目 | 配点 | 実装内容 | Farm Demoでの利用・確認方法 |
|---|---:|---|---|
| Vignetting | 3 | 画面端を減光し、中央へ視線を集める | `Vignette (+3)`。畑の選択位置や中心を見やすくする |
| BoxFilter | 3 | 周辺3x3 Pixelの平均による平滑化 | `BoxFilter (+3)`。畑やGridの輪郭が均されることを比較 |
| GaussianFilter | 5 | 距離とSigmaから求めた重みを正規化する5x5 Gaussian Filter | `Gaussian Filter (+5)`。Sigmaで平滑化の強さを調整 |
| LuminanceBasedOutline | 5 | 輝度へSobel Filterを適用して輪郭を抽出 | `Luminance Outline (+5)`。色・明度差から作物や畑の境界を抽出 |
| DepthBasedOutline | 8 | Depth Textureを線形化し、深度差へSobel Filterを適用 | `Depth Outline (+8)`。前後関係のあるObject境界を抽出 |
| RadialBlur | 5 | 指定中心から放射方向へ複数Sample | `RadialBlur (+5)`。収穫Impactのような画面演出を想定 |
| Dissolve | 4 | Noise TextureのThresholdで消去し、境界色を加算 | `Dissolve (+4)`。Threshold、Edge Width、Edge Colorを調整 |
| Random | 4 | UVとTimeからRandom値を生成し、Scene colorへ合成 | `RandomNoise (+4)`。強度、Scale、Animationを調整 |

各項目は独立したFullscreen Pixel Shaderとして実装され、Farm GamePlay Sceneを入力にして
`FinalDisplayTexture`まで描画されます。

## その他のPostEffect・技術要素

配布資料の「その他」に該当する候補です。採点時には、実装した内容とゲーム内での用途を
画面とSourceの両方で説明します。

| 実装 | 内容 | 利用目的 |
|---|---|---|
| HSVFilter | RGBをHSVへ変換し、Hue / Saturation / Valueを調整してRGBへ戻す | 収穫やRare演出で一時的に色を鮮やかにする用途 |
| Sepia | 輝度からSepia colorへ補間 | 回想・夕方・古い写真風の画面表現 |
| BoxFilter5x5 | 5x5範囲を使った強い平滑化 | 3x3とのFilter範囲比較 |
| OutlineNormal | Normal Textureの差から輪郭を抽出 | 深度差が小さい面の向きの変化を検出 |
| OutlineDepthNormal | DepthとNormalの輪郭を合成 | 形状境界をより安定して表示 |
| Bloom | 明るいPixelを拡散して元Sceneへ合成 | 発光物やImpact表現 |
| PostEffectChain | 複数Effectを固定順で連結 | 複合画面演出を1つの描画経路で構成 |
| LinearToSRGB | Linear colorを最終表示用Gammaへ変換 | Lighting/PostEffect後の表示変換を最後に1回だけ実行 |

## PostEffectChain

Chain Modeでは、ONにしたEffectを次の固定順で実行できます。

```text
Grayscale
-> Sepia
-> HSVFilter
-> Vignette
-> BoxFilter
-> GaussianFilter
-> RadialBlur
-> RandomNoise
-> Dissolve
-> OutlineDepth
-> OutlineNormal
```

中間結果は `PingTexture` と `PongTexture` を交互に使用します。入力SRVと出力RTVに
同じTextureを指定しないため、同じResourceを読み書きする不正な状態を避けています。

有効なEffectが0個の場合はCopy PassでSceneを
`PostEffectResultTexture`へ渡します。GammaCorrectionはChain内に入れず、最後に固定しています。

## 描画フローとColor Space

```text
Farm Scene
-> SceneRenderTexture（Linear）
-> 選択中PostEffect、またはPing/Pong PostEffectChain（Linear）
-> PostEffectResultTexture（Linear）
-> LinearToSRGB（1回だけ）
-> FinalDisplayTexture
-> Game Viewport / Swapchain
```

`CopyImage.PS.hlsl` と各PostEffect ShaderではGammaCorrectionを行いません。
`LinearToSRGB.PS.hlsl` だけが最後に次の変換を行います。

```hlsl
color.rgb = pow(saturate(color.rgb), 1.0f / 2.2f);
```

これにより、PostEffect途中の二重GammaCorrectionを避けています。

## 実装の責務分担

```text
PostEffectDebugWindow
  表示、Parameter入力、Preset要求
        |
        v
EditorShell
  typed actionの中継
        |
        v
PostEffectSystem
  Effect選択、値の検証・Clamp、GPU Parameter更新
        |
        v
PostEffectManager
  Pass順序、SRV/RTV、Ping/Pong、ResourceState管理
        |
        v
FullscreenPass
  RootSignature、PSO、Descriptor bind、Fullscreen Triangle draw
```

ImGui WindowがDX12 Resourceやゲーム状態を直接変更しないようにし、UIと描画処理の責務を
分けています。

## Farm Demoの基本操作

現在のSourceで確認できる操作です。

| 操作 | 内容 |
|---|---|
| Arrow Keys | 畑マス選択 |
| `1` | Hoe |
| `2` | Water |
| `3` | Seed |
| `4` | Harvest |
| `Q` / `E` | Toolを前後へ切り替え |
| `Enter` | 選択中Toolを適用 |
| `PageUp` / `PageDown` | 選択Tileの高さを上げる / 下げる |
| `Ctrl+Z` / `Ctrl+Y` | Undo / Redo |
| `Space` | Release提出版で採点用PostEffectを順番に切り替え |
| Viewportを左Click | 畑マス選択（ImGui Editor使用時） |
| `Shift` + Viewportを左Click | 選択とTool適用（ImGui Editor使用時） |
| 右Drag | Editor Camera回転 |
| 中Drag | Editor Camera平行移動 |
| Mouse Wheel | Editor Camera Zoom |
| 右Drag中 `WASD` | Editor CameraのXZ移動 |

## 主な実装ファイル

PostEffect基盤:

- `project/engine/effect/PostEffectSystem.h/.cpp`
- `project/engine/effect/PostEffectManager.h/.cpp`
- `project/engine/effect/FullscreenPass.h/.cpp`
- `project/engine/effect/RenderTexture.h/.cpp`
- `project/engine/base/DirectXCommon.h/.cpp`

評価用UI:

- `project/engine/debug/PostEffectDebugWindow.h/.cpp`
- `project/application/editor/EditorShell.h/.cpp`
- `project/application/editor/CG5DemoWindow.h/.cpp`
- `project/application/demo/PostEffectSubmissionDemo.h/.cpp`
- `project/application/demo/PostEffectSubmissionHUD.h/.cpp`
- `docs/CG5DemoGuide.md`

主なShader:

- `project/Resources/shader/Grayscale.PS.hlsl`
- `project/Resources/shader/Vignette.PS.hlsl`
- `project/Resources/shader/BoxFilter.PS.hlsl`
- `project/Resources/shader/GaussianFilter.PS.hlsl`
- `project/Resources/shader/OutlineLuminance.PS.hlsl`
- `project/Resources/shader/OutlineDepth.PS.hlsl`
- `project/Resources/shader/RadialBlur.PS.hlsl`
- `project/Resources/shader/Dissolve.PS.hlsl`
- `project/Resources/shader/RandomNoise.PS.hlsl`
- `project/Resources/shader/HSVFilter.PS.hlsl`
- `project/Resources/shader/LinearToSRGB.PS.hlsl`

## 既知の制限

- Release x64は `USE_IMGUI` を定義していないため、Parameter Sliderは表示されません。
  ReleaseではSpaceによる一定Preset比較、DevelopmentではImGuiによる詳細調整を行います。
- 実行ファイル単体ではResourceを読み込めません。実行ファイル一式へ
  `Resources`と必要DLLを含める必要があります。

## 確認結果

2026-07-23時点:

- Debug x64: Build成功、警告0、Error 0
- Release x64: Build成功、警告0、Error 0
- Required Grayscale: Farm Game Viewportで目視確認済み
- Original / Grayscale比較: 同一Cameraで切り替え確認済み
- GaussianFilter: 5x5 Shaderの`ps_6_0`コンパイル成功
- Scored Effects: Quick Check UIと実行Pass接続を確認済み
- GammaCorrection: `LinearToSRGB`で最後に1回だけ
- Game Viewport / Swapchain: `FinalDisplayTexture`を表示

## 評価動画・Screenshotの推奨構成

1. Original Colorを2秒表示
2. Required Grayscaleへ切り替え
3. `Grayscale Amount` を0から1へ変更
4. Originalへ戻す
5. 加点項目を1つずつQuick Checkから選択
6. 各EffectでParameterを1項目だけ動かす
7. Chain Modeで複数Effectが連結される様子を表示
8. 最後にGammaCorrectionがFixed Last、出力がFinalDisplayTextureであることを表示

同じCamera、同じScene、同じ画角でBefore / Afterを撮影すると、Effectによる差が伝わりやすくなります。
