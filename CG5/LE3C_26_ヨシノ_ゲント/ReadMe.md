# CG5評価課題 補足ドキュメント

## 作品概要

必須課題の Grayscale と複数の PostEffect を確認できるデモです。各 PostEffect は別画面の画像加工サンプルではなく、実際のGamePlay Sceneへ適用しています。

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


## 必須課題: Grayscale

`Grayscale.PS.hlsl` でScene colorを読み、次の輝度係数でRGBを1つの明度へ変換します。

```hlsl
float value = dot(color.rgb, float3(0.2125f, 0.7154f, 0.0721f));
```

`Grayscale Amount` によりOriginal Colorと完全なGrayscaleを補間できます。

利用方法:

- `Original Color` と `Required Grayscale` を同じCameraで比較


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

## Farm Demoの基本操作

現在のSourceで確認できる操作です。

| 操作 | 内容 |
|---|---|
| `Space` | Release提出版で採点用PostEffectを順番に切り替え |
| 右Drag | Editor Camera回転 |
| 中Drag | Editor Camera平行移動 |
| Mouse Wheel | Editor Camera Zoom |
| 右Drag中 `WASD` | Editor CameraのXZ移動 |

同じCamera、同じScene、同じ画角でBefore / Afterを撮影すると、Effectによる差が伝わりやすくなります。
