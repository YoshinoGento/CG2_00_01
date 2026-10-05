# 水路農業 タイトル演出

## 目的と範囲
固定カメラで水源、つながった用水路、段差のある畑、ニンジン・トマト・カボチャを見せる。
約40秒で朝・昼・夕・夜を巡り、入力時は約0.8秒の黒フェードを経てゲームへ移る。
既存の提出ZIPは変更しない。タイトル用の畑は独立した演出データで、保存・売買・日付進行には接続しない。

2026-10-05追加: 作物は10秒周期で成長7.5秒、成熟待ち1.5秒、空の畑1秒を繰り返す。
マスごとに0.18秒ずらし、昼夜40秒の間に4周期見せる。FarmGridは変更しない。
SystemのCropFrameを、純粋関数BuildTitleCropPartsで既存の作物形状へ変換する。
ニンジンは地中に根を残し、トマト・カボチャは既存の地上の形状で成長する。
成熟後はその場から消え、次の周期で芽から育つ。最新指示によりタイトルだけの
引き抜き/浮き上がり/縮小演出は取り消した。ゲーム中の収穫演出は変更しない。
成熟時の全パーツを初期化で確保し、毎フレームはTransform/表示フラグのみ更新。
非表示パーツはShadow/Mainの両方で描かず、0 scaleをGPUへ渡さない。
ロゴの位置と大きさは固定。文字内のランダム波紋だけを動かし、開始表示/HitTestは動かさない。
ロゴとWindow titleは「水路農業」、英字はSUIRO NOGYO。過去の提出物は旧名のまま保持する。

### 2026-10-05 検証範囲
Debug/Release x64 build、成長4周期/成熟/空き状態/固定土面/不正delta・入力/ロゴ範囲/
畑のgeneration不変/開始の一度だけの消費のCPUテストを確認。
Releaseの50秒1280x720録画で昼夜一周と静かな成長ループを確認し、
通常/最大化/Debugで文字と開始表示を確認した。約379個の畑パーツと8個の水面線を保持する。
今回の録画: generated/codex_checks/title_acceptance_20261005/suiro_timelapse01.mp4。
以前の浮き上がる収穫版の録画は最新仕様の証拠として使わない。
DebugのクリックはImGui image内の座標をタイトル1280x720へ変換して判定する。
Debug virtual1600x900の値を直接比較していた不一致を修正した。
修正後の実クリック/keyboard/gamepad、別DPI/GPU、処理時間/VRAM測定は未確認。
最新指示通り「はじめる」の位置・HitTest・点滅は維持する。

## 責務と描画順
- TitlePresentationSystem: 時間、風の目標・追従、ステージ用畑、雨粒の発生間隔・中心・寿命、開始の状態遷移。
- TitleSkyRenderer: カメラ中心の球、空・雲・天体のShader、専用PSO/CBV。
- TitleFarmRenderer: 既存の地形/作物メッシュ変換、保持したObject3d、光、水面の反射線。
- TitleView: ロゴと開始表示、開始入力の通知のみ。
- TitleLogoRippleRenderer: glyph maskと専用PSO/Root Signature/固定bufferを保持し、Systemの波紋を描く。
- TitleScene: 上記の初期化・更新・描画とSceneManagerへの遷移通知。

Shadow pass、天球、Object pass、Sprite、既存PostEffect/最終表示の順。
天球は深度を書かず、z=wで最背面に置く。既存のBarrier/Fence境界を再利用する。
モデルとTextureは各Managerから借り、インスタンス・CBV・球MeshはSceneが保持する。
毎フレームのObject/GPU buffer生成は行わない。光と共通Shadow strengthの変更は終了時に復元する。

9月30日の周辺景観追加と修正: 元の緑の地面を、農道・草地・疎らな木を含む
独立した静的OBJ一個で置換。背景の作り方と生成素材の由来は
[TitleLandscape_20260930.md](TitleLandscape_20260930.md)へ分離する。

## 空と雲
空の色はShaderの地平線色/上空色。sky.pngは雲の形と陰影の参照として使い、
青空そのものをスクロールしない。水平のUVをwrap、垂直をclampする。
雲は8～14秒ごとに擬似乱数で風の目標値を変更し、指数追従で急な方向転換を避ける。
雲の移動は40秒の昼夜ループから独立。垂直移動は小幅に制限する。
太陽・月の軌道と星の回転は昼夜時刻から計算する。天球の縦回転で雲を逆さにしない。
太陽と月は左の低い位置から昇り、上方の弧を通って右へ沈む。
Systemが画面正規化座標の中心を計算し、SkyRendererがCameraのProjectionと
回転基底を使ってworld directionへ変換する。画面構図に合わせた軌道であり、
実際の地球の回転軸や緯度に対応した天球上の大円ではない。
discの基準直径は画面高さに対して太陽14%、月8.5%。視点から離れた角度では
透視投影で実際のピクセル直径が多少変わるため、実画面でも見切れを確認する。
以前の半周期ごとのfadeと軌道リセットは廃止。両方の天体を常に動かし、
手前の不透明な地面が円を下から隠すことで沈ませる。
地面の下まで動くため、上端が隠れてから次の円が昇る間がある。
これはタイトルの構図を優先した演出で、天体の物理的な軌道シミュレーションではない。
Shaderのconstant layoutは192 bytes、既存の256-byte CBV allocation内に収まる。
検証: VS/PS6_0のDXC -WX、Debug/Release Build、連続軌道・方向・周期境界のCPUテスト成功。
分離したReleaseの44.966秒/1280×720録画で両方の軌道を確認。
0.5秒間隔の抽出フレームで、円が下から隠れ、上端まで沈んだ後に次が昇る間を確認した。
検証動画はgenerated/codex_checks/title_riseset_20260930/verification.mp4。
開始クリックで既存のゲーム導入画面へ移り、検証用プロセスは通常終了した。
左上のロゴは空より手前に描くため、左側の軌道では円の一部に重なる。
別aspect/DPI/GPUは未検証。
現時点では気象/流体シミュレーション、HDR環境光/IBL、雲の自己遮蔽ではない。

### 出入りの考え方と参考資料
実際の月は夜だけに出るとは限らず、月相で出入りの時刻が変わる。
満月はおおむね日没に昇り、日の出に沈むため、今回はその関係を参考にした。
太陽が消えた瞬間に月を生成する仕組みではなく、両方が軌道を続ける。
現実の日没は中心が地平線に達した瞬間ではなく、上端が隠れるまでを含む。
薄明もあるため、太陽の可視/不可視だけで空の色を二択にしない。
正確な月相・季節・緯度・大気差の計算は行わず、約40秒の演出へ簡略化する。
- [NASA: Moon Phases](https://science.nasa.gov/moon/moon-phases/)
- [NWS: Sunrise and Sunset](https://www.weather.gov/fgz/Seasons)
- [NWS: Twilight](https://www.weather.gov/fsd/twilight)

## タイトルの作り方
静止画一枚や録画動画を背景にしているのではなく、毎フレーム3D背景を描いている。
役割ごとの処理は次の通り。

### 1. タイトル専用の畑を用意する
TitlePresentationSystem::Initializeで5列×4行のFarmGridを作る。
奥から高さ2、1、0、0の段差を作り、中央の列を水源と用水路にする。
左右にはニンジン・トマト・カボチャを配置する。
これは見せるための畑で、プレイヤーの保存した畑ではない。
水分と畑のデータは展示用の固定値。成長/収穫は描画専用のループで、実際の収穫・売買・セーブは発生しない。

### 2. 40秒の時計から演出値を作る
TitlePresentationSystem::Updateが実時間のdeltaを受け取り、40秒で一周する時計を進める。
NaN/Infinity/負のdeltaは無視し、一度の更新は最大0.25秒に制限する。
朝、昼、夕、夜の基準色を10秒ずつ補間する。
補間はsmoothstep相当の式で、区切りで急に色が変わらないようにする。
同じ時計から光の色/強さ、水の色、葉の揺れ、開始表示の明滅を作り、Frameへまとめる。
Frameは演出値の受け渡しで、UIが時計や農場の状態を変更する場所ではない。

### 3. カメラの周囲に天球を置く
TitleSkyRendererは48×24分割の球を初期化時に作り、カメラ位置を中心に描く。
球の位置はカメラに追従するが、球全体は縦回転させない。
VSではz=wにして最奥へ描き、Depth bufferには書き込まない。
手前の畑と地面は後で描くため、空の前に正しく重なる。

### 4. 空・雲・星をShaderで描く
上空色と地平線色を、見ている方向の高さで混ぜて空の色を作る。
専用の生成画像sky.pngは雲の形/陰影の参照に使い、青空の色はShader側で作る。
雲を読むUVに風のoffsetを加える。風の目標は8～14秒ごとに変わり、滑らかに追従する。
Point samplingと固定のangular texelで少しドット絵らしい輪郭を残す。
星はShader内の疑似乱数で配置し、夜の量と雲の量で見え方を変える。
空・太陽・月・星を作ってから雲を手前へ合成する。以前は雲の後に円を上書きしていたため、
太陽/月が雲より手前に見えていた。雲の濃度と地平線付近のgateを掛けたopacity0..1で
濃い雲は円を隠し、薄い境界は透かす。星も同じ雲の最終合成で遮蔽し、二重の減衰はしない。
新しいCloud passやDepth/Descriptor/Barrierは追加しない。同じ天球Shader内の色合成である。
2026-10-01: DXC PS6_0(-WX)、Debug/Release x64 build、合成順とopacityのCPU reference testを確認。
別コピーのReleaseを45秒録画し、雲が太陽/月の手前を通る表示を確認した。
CPU testはGPU readbackではなく、体積雲や雲が地面へ落とす影は対象外。
空全体を粗い色段階に丸めず、固定の小さなditherでUNORMのbandingを抑える。

### 5. 太陽・月を連続した弧で動かす
画面左上を(0,0)、右下を(1,1)として、次の楕円を使う。

```text
angle = 2*pi * time / 40
x = 0.50 - 0.42*cos(angle)
y = 0.58 - 0.48*sin(angle)
moonAngle = angle + pi
```

太陽はおおむね左下→上中央→右下、月は半周期ずれて同じ方向へ進む。
下半分も計算するので、周期の境目で位置を瞬間移動させない。
SkyRendererが画面位置をCameraのProjectionと回転からworld directionへ変換する。
PSはその方向に近い画素を円として描く。実際の球モデルを遠方へ置く方式ではない。
地面に重なった部分は隠れるが、天体自体のalphaは下げない。
月面には固定lunar-local座標で簡略化した海(暗い領域)・crater・細かな濃淡を描く。
雲は円より手前で濃度に応じて遮蔽する。満ち欠けは未実装。正確な月面地図ではない。

### 6. 畑の小さな動きを加える
TitleFarmRendererが既存の地形/作物パーツをObject3dへ変換し、固定カメラで描く。
葉や茎を小さく揺らし、水面には8本の反射線を移動させる。
水が流れるように見せる演出であり、タイトルでは水量の流体計算をしない。
TitleCelestialGeometryの共通ray変換をSkyRendererとFarmRendererで使用する。
太陽を指すworld directionを反転した値がDirectional lightの光線方向になる。
太陽の高さ成分が0.05以下では影を消し、0.05から0.16でsmoothstepにより0.70まで強める。
影がある時間帯にはライトの方向は太陽と厳密に逆向き。地平線付近では天空の補助光へ滑らかに戻す。
夜は月側の薄い補助光だけで、cast shadowは0。終了時に元のlight/Shadow strengthを復元する。

#### 朝夕で一瞬明るくなる不具合
16-25-02の動画で、地平線付近の地面が昼より明るくなることを確認した。
方向を太陽側から上方向の補助光へ戻すと、同じintensityでも水平面のNdotLが急増するため。
Systemは空の補間と同じ時計で、水平地面の露出を朝0.25/昼0.82/夕0.25/夜0.10でsmoothstep補間する。
Rendererは実際のray方向から次を計算する。

```text
groundResponse = 0.18 + 0.82 * max(-lightDirection.y, 0)
directionalIntensity = groundExposure / groundResponse
```

係数は既存Object3D.PS.hlslのambient/diffuseと対応する。共有Shaderは変更しない。
水平な非遮蔽地面では方向が変わっても受ける明るさはgroundExposureとなり、逆転を防ぐ。
斜面・木・作物には通常のnormal shadingとShadowが残る。これはタイトルの露出補正であり物理的な照度モデルではない。
Shader側のambient/diffuse係数を変える場合、この対応式も見直す必要がある。
CPU検証では一周期の方向一致、夜の影0、有限intensity、水平面の受光値と露出の一致、
各10秒区間の露出の単調性、周期境界を確認。Debug/Release BuildとDXC VS/PS6_0 -WX成功。
最新の分離Release録画verification03.mp4(44.966秒/1280x720)で、昼が明るく夜が暗い推移を確認した。
固定草地ROI(x100/y650、100x50)の10Hz YAVGは57.4662～147.445、隣接100msの最大差2.646。
これは録画上の一領域の診断値であり、全画面の無ちらつき/測光/性能保証ではない。
開始操作から既存のゲーム導入へ遷移し、検証用プロセス36552は通常終了(exit0)。

NASAでは実際のSun/Moonの見かけの大きさは近く、満月は影を作ることもあると説明している。
月を小さくすることと夜の投影影を消すことは、今回の見分けやすさとHD2D演出の選択。
- [NASA: Angular Size](https://science.nasa.gov/wp-content/uploads/2023/09/Electromagnetic_Math.pdf)
- [NASA: Moonlight](https://science.nasa.gov/moon/moonlight/)

### 7. ロゴと開始操作を重ねる
TitleViewがロゴ、開始表示、黒fadeのSpriteを描く。
SPACE/ENTER、Gamepad A、開始文字のクリックを開始要求として通知する。
Systemが約0.8秒の退出fadeを管理し、完了時に一度だけ開始通知を返す。
TitleSceneがSceneManagerへGAMEPLAY遷移を通知する。ViewはSceneを直接切り替えない。

#### 控えめなドット風ロゴ（2026-10-05）
「水路農業」と開始文字に加え、最新の指定で英字も2px単位の輪郭と塗りに統一。
既存Noto Sans JPを47pxで228x84のmaskへ描き、alpha128以上を塗りとして二値化。
Nearest Neighborで456x168へ拡大し、元の位置とサイズで使用する。
低解像度maskにMaxFilter(3)をかけてから拡大し、輪郭も2pxの階段に揃える。
新しいpixel fontの導入ではなく、既存fontのraster表現の変更である。
波紋用glyph maskも同じ字形から生成するため、反射が文字外にはみ出す問題を避ける。
開始日本語は15px/140x40のmaskから280x80へ拡大する。
`SUIRO NOGYO` は10px/228x84、`SPACE / ENTER / A` は10px/140x40のmaskから2倍拡大。
既存fontのweight600で、小さい英字のstrokeを残す。subtitleは(16,130)、キー案内は(140,48)中心揃え。
キー案内は16px相当から20px相当へ大きくしてslashを判別しやすくした。画像全体のサイズは変えない。
点滅式(.82+.18*cos、2.5秒周期)、画像サイズ、表示位置、開始領域は変更しない。
`tools/title/generate_title_text.py --logo-only` でlogoと波紋だけを再生成する。
`--reflection-only` との同時指定は拒否する。どちらも開始画像には書き込まない。
文字画像のGPUサイズは変えない。波紋用Textureと描画は次節の通り変更する。
2px grid/4文字のstroke量/波紋の文字内制限/phaseの連続性/昼夜contrastの素材検査は成功。
Debug/Release x64 Build、従来の点滅・crop/timing regressionは成功。
独立Releaseのpixel_logo02.mp4（41.966667秒/1280x720/1211frames）と通常サイズの昼夜画像で、
文字の判別・階段状の輪郭・波紋が新字形内に収まる表示を確認。
証拠はgenerated/codex_checks/title_pixel_logo_20261005/、6秒の抜粋はpixel_preview02.mp4。
上記pixel_logo02はロゴのみを変更した時点の証拠。「はじめる」のドット化は、その後の変更。
最新の素材検査では、英字も含む画像全体の2px gridと独立生成した英字mask/outline、
文字boundsと各英字・slashのstroke残存を検査。英字変更時には日本語領域・旧波紋Atlasが不変だった。
英字変更はPNGとoffline generatorのみ。C++/Shader/Descriptor/Barrier/Fence/DrawCallは変更しない。
この素材変更では再compile不要。Release Buildも成功。
独立Releaseの `generated/codex_checks/title_pixel_english_20261005/full01.png` でsubtitleと
16px相当のキー案内を確認後、slashを判別しやすくするため最終20px相当へ改善した。
録画は途中で終了（2.533333秒）。原因は未特定。最終20px案内は下記rain01の通常サイズ画像でも確認した。
古いpixel_logo02およびfull01は最終20px案内の表示証拠ではない。
別DPI/非整数縮小/Debug dock内のpixel鮮明さは未確認。既存linear samplerは変えていない。

#### 雨粒風のランダム波紋（2026-10-05）
固定中心・10秒周期のAtlas演出は取りやめた。雨粒が文字へ落ちるように、中心から外側へ広げる。
初回は0.9〜1.5秒、その後の発生間隔は2.6〜4.2秒。2回の一様乱数の平均で、極端な間隔を少なめにする。
1波紋の寿命は3.2秒、固定配列2個で最大2波紋まで重なる。寿命<最短間隔*2をstatic_assertする。
発生時刻まで既存波を進め、空いたslotに新しい波を作り、残りのフレーム時間を進める。
Frame末尾で先にslotを消す方式では、フレーム時間により選ばれるslotが変わるため、この順序へ修正した。
大きいdeltaは従来通り0.25秒に制限する。停止後の積み残しを一斉発生させない。
中心候補は日本語maskのopaque pixelから生成した64点（各文字16点）。直前と同じ候補の連続選択を避ける。
`TitleLogoRipplePoints.h` はgeneratorで再生成する。文字外・輪郭・英字には発生させない。
雨のPRNGは風とは独立。通常起動はsteady_clock由来seed、テストでは明示seedで再現する。
これは見た目用の疑似乱数であり、暗号や厳密な物理的降雨の分布ではない。落下する粒子自体は追加していない。
Systemが中心、半径、寿命opacityをFrameへ渡す。Viewは時間・抽選を扱わずRendererへ通知する。
Shaderが幅12pxのGaussian波頭と40px遅れる45%の波頭を描く。縦方向は1.6倍の楕円距離、2px単位。
出入り各0.45秒はsmoothstep、昼最大opacity0.86/夜0.64。重なりは最大opacityを採用し、文字を濃くしすぎない。
色は(.02,.36,.82)。同じ字形の456x168 maskをpoint samplingし、塗りの内側だけに描く。
旧32コマAtlas（base9.35MiB）からmask1枚（base約0.29MiB）へ変更。mips/alignment/実測VRAMは含まない。
専用PSO/Root Signatureを初期化時に作り、vertex/index/256-byte CBの3 bufferを保持。
2波紋とも1 DrawCallで描く。休止時は0。毎フレームの動的確保・Texture upload・Descriptor作成はない。
既存TextureManagerからmaskを借りる。RendererのComPtrがbuffer/PSOを所有し、Scene寿命内で使う。
現行PostDraw Fence待機後のCB再利用を前提とする。複数frame in flightにする場合はbuffer ringが必要。
描画後はTitleViewがSpriteCommon::PreDrawを呼び、開始文字/暗転用PSO・Root Signatureを戻す。
共通Sprite Shader/Sampler、Barrier/Fence、Game/Scene/保存・提出ZIPは変更しない。
Shaderの演算は増えるため、Texture容量の減少だけで速度改善とは断定しない。実測CPU/GPU負荷は未計測。
検証:600秒・12種seed・混在delta・異なる刻み幅で、間隔/発生数/中心/最大2/空白上限/再現性/風独立を確認。
不正delta、再Initialize、開始点滅/一度だけの遷移、昼夜/cropも回帰検査。64候補のmask所属と
独立CPU参照による外向きの波/昼夜contrastを検査。GPU保証とは区別する。
DXC VS/PS6_0 -WXとDebug/Release x64 Build成功。
独立Releaseのrain01.mp4（32秒/1280x720/853frames）で、異なる中心・外向きの進行・英字最終サイズを確認。
証拠は `generated/codex_checks/title_rain_ripples_20261005/`。rain_preview01.mp4は1〜13秒の未拡大抜粋。
rain01はslot処理順の最終修正前。最終修正後の独立Release runtime02でrain02.mp4（19.966667秒/1280x720/547frames）を撮影した。
半秒間隔のsequence_final.pngと通常サイズfull_final.pngで、中心の変化・外向きの進行・文字内への制限・英字最終サイズを確認した。
rain_preview02.mp4は最終版から12秒を抜粋。記録場所はgenerated/codex_checks/title_rain_ripples_20261005/。
これは現在のGPUでのタイトル表示確認。最終版の全夜景や実入力、GPU負荷の計測を確認したことにはしない。
旧radial02/visible05/pixel_logo02はランダム版の証拠として使わない。
再生成は `tools/title/generate_title_text.py --reflection-only`。logo/startは更新しない。
他GPU/DPI、Debug dock、実操作による開始遷移、実測VRAM/フレーム負荷、全エンジン同期は未検証。

### 現在の制限
小鳥・トンボ・満ち欠け、完成したHD2Dのモデル/背景は未実装。
既存のPostEffect経路は通るが、専用のBloom/DOF/Color gradingを追加した構成ではない。
376個の畑パーツ、景観一個、8本の反射線、Shadow passがあるため、DrawCall負荷は今後計測する。
保持したGPU resourceを再利用するが、全エンジンの同期/VRAM安全性を保証する検査ではない。

### 色変化時の帯の修正
9月30日の動画で、昼夜の色変化に合わせて空に等高線状の帯が動く問題を確認。
原因はTitleSky.PS.hlslのRGB全体を約48段階に丸める処理で、
連続した空の勾配に人工的な境界ができ、補間中にその境界が移動していたこと。
雲のangular texel/point samplingは残し、空全体の色の段階化は削除した。
R8G8B8A8_UNORM出力による残りのbandingを抑えるため、画面位置で決まる
4x4 Bayer ordered ditheringを追加。各成分の振幅は0.5/255未満、
パターンの平均はゼロで、時刻やフレーム番号による模様の点滅はない。
新しいGPU resource/Descriptor/Barrier/Fence、毎フレームの動的確保は追加しない。

検証: DXC ps_6_0 warnings-as-errors成功、Debug/Release x64 Build成功。
CPU参照式で16順位の一意性、平均ゼロ、振幅制限、UNORM化後の空間平均誤差を確認。
参照式の検査はGPU readback試験ではない。
独立したReleaseコピーの暗い空と暖色への移行画面で、以前の太い帯がないことを目視確認。
録画圧縮後や別GPU/ディスプレイでのbandingまでは保証しない。

## 素材の制作方法
ユーザーの確認後、無料素材案を取りやめ、専用の生成素材で試作。
OpenAI built-in imagegen、参照画像なし。手描き作品として申告しない。
生成結果はResources/title/sky.pngへコピーし、元データは保持する。
文字は既存Noto Sans JPから決定的にラスター化する。

### 最終生成プロンプト
Use case: stylized-concept. Asset type: ORIGINAL 2:1 equirectangular SKY-ONLY texture for an HD-2D farming game's rotating sky sphere, landscape 2048x1024. Create a seamless horizontally wrapping 360-degree sky panorama, zenith at top edge, mathematical horizon at exact center row, nadir at bottom edge. No ground or mountains whatsoever. Upper half: calm natural blue daytime sky with realistic volumetric soft cumulus clouds, clearly drawn small pixel clusters and subtle restrained dithering, like a hand-painted pixel-art landscape with a little realism. Preserve convincing cloud anatomy and depth, NOT photorealism, NOT tiny noisy photographic detail, NOT chunky retro 8bit. Lower half: extremely simple pale blue, sky only, almost empty, smooth at the horizon. Scattered clouds occupy upper quarter to slightly above center, with big clear-blue gaps. Horizon band sparse and unobstructed. Left/right edges must match color and cloud structure so it wraps; topmost edge plain uniform blue for the spherical pole. Neutral daytime illumination without a dominant light direction. NO sun, moon, stars, birds, text, UI, logos, lens effects, land, skyline, black border or collage. One continuous flat unframed texture, not a rendered sphere preview. Color palette subdued blue and white-gray, highlights not blown out. It should retain stylized pixel brushwork when viewed against low-poly terraced farmland. The game will animate time colors and celestial bodies separately.

## リスクと検証予定
夜の可読性、背景と作物の絵柄差、雲の端の接続、UIの見切れ、光の復元、
大量の地形パーツによるDrawCallを確認する。速度改善や全体のDX12正当性は計測なしに主張しない。
CPUテスト・Debug/Releaseビルド・実画面は別々に検証結果を記録する。
