# 水道農業 タイトル演出

## 目的と範囲
固定カメラで水源、つながった用水路、段差のある畑、ニンジン・トマト・カボチャを見せる。
約40秒で朝・昼・夕・夜を巡り、入力時は約0.8秒の黒フェードを経てゲームへ移る。
既存の提出ZIPは変更しない。タイトル用の畑は独立した演出データで、保存・売買・日付進行には接続しない。

## 責務と描画順
- TitlePresentationSystem: 時間、風の目標・追従、ステージ用畑、開始の状態遷移。
- TitleSkyRenderer: カメラ中心の球、空・雲・天体のShader、専用PSO/CBV。
- TitleFarmRenderer: 既存の地形/作物メッシュ変換、保持したObject3d、光、水面の反射線。
- TitleView: ロゴと開始表示、開始入力の通知のみ。
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
成長や水分は展示用の固定値なので、待っていても収穫・売買・セーブは発生しない。

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
満ち欠け・雲による天体の遮蔽はまだ実装していない。正確な月面地図ではない。

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
TitleViewがロゴ、開始表示、黒fadeの3枚のSpriteを描く。
SPACE/ENTER、Gamepad A、開始文字のクリックを開始要求として通知する。
Systemが約0.8秒の退出fadeを管理し、完了時に一度だけ開始通知を返す。
TitleSceneがSceneManagerへGAMEPLAY遷移を通知する。ViewはSceneを直接切り替えない。

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
