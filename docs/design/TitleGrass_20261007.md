# タイトルの野草と草波

## 目的と参考
角柱の草を撤回し、先細りで曲がる野草が面としてつながり、風の波が通過するタイトル背景へ変更する。
畑・水路・農道を隠さず、ドット感は四段階の色帯と控えめな葉幅の段階化で付ける。
現在の対象はタイトルだけ。プレイ用農場への配置や草刈り・衝突判定は含まない。

見た目は[任天堂の公式WORLD映像](https://www.nintendo.com/jp/zelda/botw/world.html)、
[DQ11の公式フィールド画像](https://www.dq11.jp/stage/index.html)、
[公式オープニング映像の抜粋](https://www.youtube.com/watch?v=KdHYS2OWYW0)を参考にした。
細い葉の曲がり、丈と向きのばらつき、群生した輪郭を観察して、本作に合わせて構成した。
両作品の内部実装を調べられたわけではない。映像・画像・モデル・音楽はゲームへ取り込まない。

技術参考は[NVIDIA GPU Gems Chapter 7](https://developer.nvidia.com/gpugems/gpugems/part-i-natural-effects/chapter-7-rendering-countless-blades-waving-grass)。
Vertex Shaderで葉を変形する考え方を参考に、既存DX12基盤に専用のInstancing描画を追加した。
参考資料に掲載された他作品のアルゴリズムをそのまま再現したという主張はしない。

## 責務
- `generate_title_grass.py`: 元の草OBJを生成する。96草束を層化した方形Patchへ配置。近景は各5葉・3分割、遠景LODは各3葉・2分割。生成はオフラインのみ。
- `grass_layout.json`: 703か所の位置・大きさ・色。畑の余白と農道を最大風変形込みの保守的なBoundsで避け、固定タイトルカメラ外の候補を除外。箱庭中心から24以内の94か所を近景・Shadow用の先頭区間に並べる。
- `TitlePresentationSystem`: 雲の風速から草の風向き・強さと、草波の位相を更新する。SceneやUIへタイマーを置かない。
- `TitleGrassRenderer`: ManagerからModelと影SRVを借り、Instance VB、色用/影用の別CB、PSOを所有する。畑Rendererは描画順を統括するだけ。
- `TitleGrass.VS.hlsl`: 同じ変形を通常表示とShadow passへ適用する。`TitleGrass.PS.hlsl`は両面の葉に既存の昼夜光・影と四段階の色を適用する。

## 草波の処理
草の風は雲が見かけ上流れる世界座標の軸に合わせる。天球UVの縦方向の動きは草の風向きへ流用しない。
風速を基準値0.0015で正規化して制限し、草の最大変形係数を0.68にする。
位相は風速に応じて進め、逆風では逆へ進める。4秒の位相周期で一周してもsin入力は連続する。

```text
phase = timePhase - dot(worldXZ, windAxis) * 0.85 + smallTuftOffset
crest = 0.5 + 0.5 * sin(phase)
gust = 0.08 + 0.92 * crest^2
bend = worldWind * height^2 / 0.35 * gust
```

`worldXZ`が違う草は異なる時刻に強く倒れるため、帯が草地を横切る。すべての草を同時に振らない。
高さの二乗で変形量を増やすので根元の位置と接線は固定され、葉先ほど揺れる。
法線は変形JacobianのInverse-transposeで補正する。位置によって波の強さが変わるため、
高さ方向だけでなくXZ方向のgust勾配も含める。補正の分母は出荷メッシュと風上限で非ゼロになることをCPU参照で確認する。
通常表示と影は同じVSを使い、CBのViewProjectionだけを変える。影用CBへ書いた行列が通常表示に残る事故を、別CBで防ぐ。

## 地形と照明の修正（2026-10-09）
草原をZ約81、X約±46まで広げる。草丈は0.24～0.46、Scale込み最大0.483。
既存の木は高さ係数1.8～3.6・樹冠上端約2.6～5.2なので、木に対して十分低い野草にする。
奥の起伏は解析式だけでは実際の地面と一致しない。Landscapeの5単位Gridの対角線と同じ三角形の
Barycentric補間で、各Vertexを地面へ合わせる。法線も地形Shear、その後の風変形の順に補正する。
地形生成式はLandscape generatorとVSに対応するため、地形Gridや式の変更時は両方とテストを更新する必要がある。

以前は水平地面専用の光強度補正（Exposure / (.18 + .82*NdotL)）を、両面草葉にも使っていた。
異なる葉法線と照明式に同じ補正を掛けるため、地平線付近で草だけ明るさが増幅された。
現在は`GrassExposure(frame.groundExposure)`を使用し、空からのFill90%・方向光最大10%に制限する。
Exposure自体は既存の連続した昼夜補間。太陽・月の方向と昼夜のShadow strengthは既存系統を維持する。
草波は倒れ方の差を拡大し、色にも最大20%の穏やかな差を付ける。物理的な光輸送の再現ではなく、
昼夜の破綻を抑えたタイトル用のStylized lightingである。

## 実行時安全性と負荷
草はOpaqueの両面薄葉で、透明TextureのDepth write・ソートは不要。ただし重なった草のPixel処理は残る。
通常表示は近景・遠景の2回、Shadow passは近景だけ1回のInstancing描画。
近景94×2,400=225,600、遠景609×864=526,176で通常表示751,776三角形。
Shadow込み977,376三角形/Frame。前回の479,808より増加しているため、LOD導入だけで「軽くなった」とは主張しない。
すべて近景Meshなら通常表示1,687,200三角形になるため、遠景LODとShadow制限で増加を抑えている。
Instance VBは初期化時だけUpload、更新中はCBの書き換えだけ。フレームごとのモデル読み込み・動的確保・Descriptor追加はしない。
専用のRenderTargetやShadowMap、Barrier/Fence経路は増やさない。既存ShadowMapの状態遷移とPostDraw Fenceの直列実行に依存する。
この前提を変えてFrames-in-flightを導入するときは、CBのリング化とRenderer寿命を再設計する。
1,024Instance上限とfinite/range/Shadow区間検査で不正JSONを拒否する。
Instance VBは703×48=33,744bytes。近景と遠景は同じVBの先頭とOffset付きViewを参照する。
Root Signature、VB stride、128byte CBの256byte配置を固定する。追加LOD Modelの所有は既存Manager、Rendererは借用する。
将来位置を編集したら配置検査を再実行する。ランタイムJSON検査は型・範囲検査であり、畑との幾何交差を再計算する機能ではない。
GPU時間・FPS・実VRAMは未計測。全ゲームへ広げる前にはDistance LOD、可視範囲・Shadow範囲の間引きとProfilingが必要。

## 検証
`title_grass_asset_test.py`で固定seed、OBJのindex/UV/winding/単位法線、全草の最大風変形Bounds、畑/農道への非交差、
根元固定、草波の移動方向、数値微分との法線整合・Jacobianの非折り返しを確認する。
`title_presentation_test.cpp`で雲の風速と草の強さ・位相進行の一致、既存タイトル600秒の回帰を確認する。
独立Release録画とDebug/Releaseビルドは実施結果を作業記録へ分けて残す。
元セーブ・提出ZIPは変更しない。草波の見え方の好み、別GPU/DPI、長時間の性能は別の確認事項。

2026-10-07版の確認: Debug/Release x64、DXC VS/PSの`-WX`、上記CPUテストは成功。
`generated/codex_checks/title_grass_20261007/wave04.mp4`は45秒の独立Release録画で、
昼夜の草の明るさ・密集した葉の場所ごとの揺れを確認した。全フレームのDecodeも成功。
短い確認動画は同じフォルダの`grass_wave_preview04.mp4`。録画の60fps出力設定は実ゲームの60fps保証ではない。

2026-10-09版: Debug/Release x64、DXC VS/PS `-WX`、配置/地形補間/斜面と風の合成法線、
タイトル80秒の草Exposure連続性と既存600秒回帰は成功。
独立runtime01は起動後に画面確認前に終了。直近Application Errorには対象ゲームのCrash記録は見つからず、原因未確定。
再起動操作はツール未承認で停止し、ユーザーへ確認中。今回の見た目・GPU性能はまだ未検証で、以前の動画を今回の証拠として使用しない。
