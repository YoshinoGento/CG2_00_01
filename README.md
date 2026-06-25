[![DebugBuild](https://github.com/YoshinoGento/CG2_00_01/actions/workflows/DebugBuild.yml/badge.svg)](https://github.com/YoshinoGento/CG2_00_01/actions/workflows/DebugBuild.yml)
[![ReleaseBuild](https://github.com/YoshinoGento/CG2_00_01/actions/workflows/ReleaseBuild.yml/badge.svg)](https://github.com/YoshinoGento/CG2_00_01/actions/workflows/ReleaseBuild.yml)
[![DevelopmentBuild](https://github.com/YoshinoGento/CG2_00_01/actions/workflows/DevelopmentBuild.yml/badge.svg)](https://github.com/YoshinoGento/CG2_00_01/actions/workflows/DevelopmentBuild.yml)

# Farm Effect Demo

農業箱庭ゲームを想定した、畑操作と収穫演出のデモです。
3x3の畑マスに対して、耕す、水やり、植える、成長、収穫の流れを実装しています。

収穫時にはGPU Particle、Floating Text、PostEffectChainを組み合わせて、動画で見たときに収穫したことが分かりやすい演出になるようにしています。

# 主に使用した技術

## GPU Particle

耕した時の土ぼこり、水やり時の水しぶき、収穫時の粒子演出に使用しています。

また、Particle Interactionでは、選択した畑位置を中心に粒子を外へ拡散したり、中心へ吸収したりする処理をGPU側で行っています。

## Harvest Impact Effect

収穫時に発動する演出です。

収穫位置にパーティクルと `+120G` を表示し、収穫したことが分かるようにしています。

## Digital Impact Effect

Rare収穫時の特別演出として使用しています。

通常収穫より目立つように、青系のデジタル風エフェクトとポストエフェクトを組み合わせています。

## PostEffectChain

複数のポストエフェクトを順番に適用する仕組みです。

Digital Impact時に、RadialBlur、HSVFilter、RandomNoise、Vignetteなどを使用しています。

## FloatingTextSystem

収穫時の `+120G` や、畑操作時の「耕した！」「水やり！」「植えた！」「収穫！」を、World座標に紐づけて表示するために使用しています。

画面固定UIではなく、対象の畑マス上に表示されるようにしています。

## Auto Farm Demo

`F5` キーで、畑操作の流れを自動再生できます。

評価動画用に、畑を耕す、水やり、植える、成長、収穫、演出発動までを順番に見せられるようにしています。

# 操作方法

| キー | 内容 |
| --- | --- |
| 左クリック | 畑マス選択 |
| T | 耕す |
| Y | 水やり |
| U | 植える |
| I | 収穫 |
| J | Rare演出 |
| 1 | Particle Interactionの発生位置を左の畑に変更 |
| 2 | Particle Interactionの発生位置を中央の畑に変更 |
| 3 | Particle Interactionの発生位置を右の畑に変更 |
| K | Particle Interaction表示ON/OFF |
| O | 選択位置から粒子を外へ拡散 |
| P | 選択位置へ粒子を吸収 |
| F2 | HUD表示切り替え |
| F5 | Auto Farm Demo |
| F6 | Skybox切り替え |
| F7 | Presentation Mode切り替え |
| F11 | 全画面切り替え |
| WASD | カメラ移動 |
| Q / E | カメラ上下移動 |
| 右ドラッグ | カメラ回転 |

# 評価動画向け機能

## Presentation Mode

評価動画撮影用に、Debug UIを非表示にしてゲーム画面を見やすくするモードです。

`F7` で切り替えできます。

## Particle Interaction

ReleaseビルドでもImGuiに依存せず、キー操作で表示できます。

`1 / 2 / 3` で発生位置を選び、`K` で表示、`O` で拡散、`P` で吸収を行います。

## Gamma Correction

描画結果は `FinalDisplayTexture` にまとめ、最後に `LinearToSRGB` によるGamma補正を1回だけ行っています。

Game ViewportとSwapchainは、Gamma補正後の `FinalDisplayTexture` を表示します。
