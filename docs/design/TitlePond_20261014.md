# タイトルの池と水滴による画面遷移

## 目的と範囲

手前の池に畑・空・文字を映し、タイトル背景のクリック位置から水滴が落ちる。
着水した波紋で反射を歪ませ、その波が画面全体を覆う。次の画面は外周から戻る波の通過後に見せる。
参考画像へ構図を近づけるタイトル専用の表現で、プレイ用農場の地形・水管理・保存内容は変更しない。

2026-10-15の改善で、固定線を境に画面を上下反転する方式から、専用の低解像度
Planar Reflectionへ変更した。水面の高さhに対してworld位置のYを `2*h-y` に反転して描く。
これにより、画面内画像の貼り替えではなく、木・展示作物・揺れる草を実際の位置関係で映す。
反射Rendererの初期化に失敗した場合だけ従来のScreen-space Mirrorへ戻す。
水面合成自体は固定カメラ用で、屈折先のDepth追跡や流体計算は行わない。
水面上のタイトル文字は読みやすさのための演出的な合成であり、3D物体の反射ではない。

## 責務

| 対象 | 担当 |
| --- | --- |
| TitlePondSystem | JSON 読込、画面座標と池平面の対応、着水位置、開始要求の一回性 |
| TitlePondRenderer | PSO・Root Signature・snapshot Texture・SRV の所有、水面合成 |
| TitleReflectionRenderer | 640x360の反射用Color/Depth、RTV/DSV/SRV、専用Skyの所有と鏡像描画 |
| TitleFarmRenderer | 展示畑・導水路の描画配置、Shadow・反射・本描画の順序。実農場は変更しない |
| TitleGrassRenderer | 本画面・Shadow・反射で別々の定数バッファを使い、反射には低詳細モデルを使用 |
| TitlePresentationSystem | タイトル畑の段差・展示作物・昼夜・点滅。実農場の状態とは別 |
| TitleAudioSettingsSystem | 設定画面で入力を消費し、それ以外の有効なクリックを開始通知へ変換 |
| TitleView | ロゴと「はじめる」の表示。開始可否や地形変更は判断しない |
| TitleScene | 更新・描画の順序と要求の受け渡し。池初期化に失敗した場合は通常の遷移へ戻す |
| SceneTransitionSystem | 水滴の時間・SE・Cover/Reveal・描画済み確認・入力解放待ち |
| RippleTransitionRenderer と Shader | 水滴、三つの波束、屈折・ぼかし・画面を覆う合成 |

Game とプレイ用 Farm System には今回の状態変更を追加しない。
既存の GamePlayScene/Controller の大きさと責務混在は、このタイトル変更で解消したとは扱わない。

## 座標と素材

`Resources/title/pond_layout.json` に池の中心・半径・岸幅・fallback反射境界・導水路3点・半幅をまとめる。
Generator はこの設定から岸を作り、草を除外する。設定を変える場合は岸・草の再生成と配置テストも必要。
農道は池の外へ逃がす。展示畑は手前を低く、奥を高くし、水源を奥へ配置する。

池の中心 `(2.8,-0.29,-1.4)` と半径 `(5.2,3.3)` は従来のまま。
展示畑だけを岸へ1単位寄せ、奥の水源から段差のある水路を通り、手前の開口部から池へ流れる形にした。
導水路は `(0,.145,2.64)` → `(0,-.17,1.52)` → `(0,-.29,1.15)` と下り続ける。
入口では展示畑の終端の壁だけを開き、出口では池の石岸に切れ目を設ける。
これは展示用の地形構成であり、実農場の水量シミュレーションとは接続しない。
設定の型・有限値・下り勾配・池内の出口・水面との高さ一致をSystemで検証する。

画面 UV と池の正規化 XZ 座標の関係を Homography で表す。
描画用の View Projection から平面の 3x3 射影を作り、double で逆行列を計算し、float9個を保持する。
カメラの向きから別途近似した Ray では、描画座標と着水点にずれが生じたため、この方式に変更した。
特異行列・非 finite・見えない側の除算を拒否し、着水点は池半径の78%以内、画面内へ制限する。

`generate_title_pond.py` は3017 trianglesの不揃いな石岸・導水路と128x64の石atlasを固定seedで生成する。
面順序と上向き面の法線を検査する。`generate_title_landscape.py` は本描画と同じ木・柱から
`reflection_scenery.obj` を生成する。反射では地面の裏面を描いて空を隠さないよう、草原の地面を除く。
木は幹と非対称の樹冠で構成し、草の配置は変更後のカメラと池の除外範囲に合わせて再生成する。
素材はコード生成で、写真の複製や第三者モデルではない。Codex 補助による生成であり手作業モデリングとは申告しない。

## 描画と遷移

1. TitleFarmRendererがShadowを更新し、専用RTへ空・木・展示畑・作物・草の鏡像を描く。本画面をその後に描く。
2. ロゴを描き、Scene Render Targetを別snapshotへCopyResourceする。
3. 池のHomography mask内に反射RTを合成する。微細な波・流入口の同心波・岸際の浅瀬色・弱い泡とハイライトを加える。
4. 「はじめる」の点滅と設定画面を描く。設定画面自体は反射に含めない。
5. クリック後は0.32秒で着水する軌道を描き、着水時の SE を通知する。
6. 同じ広がる波の先端を屈折と Cover で共有する。最初は池内の楕円状、後半は画面全体へ広げる。
7. Cover 濃度は進行度0.20から0.58で滑らかに上げ、初期の複数波紋を隠さない。池用の境界幅は16px。
8. 全面を覆った描画の確認後に Scene を交換し、戻る波の外側から次画面を見せる。入力解放まで操作を渡さない。

水面を直接クリックした場合も短い落下距離を確保する。
設定ボタン・モーダルはクリック開始に流さず、Enter/Space の開始も維持する。
種屋の短い Fade と他画面の既存 Ripple は池用設定を無効にして従来の動作を保つ。

## DirectX12 と負荷

snapshotと反射RTは各Renderer所有、Device/CommandList/SrvManager/Model/Textureは借用する。
初期化時に Texture と SRV を確保し、毎フレームの動的確保や Model 読込はしない。
Scene RT を COPY_SOURCE、snapshot を COPY_DEST にしてコピーし、Scene RT を RENDER_TARGET、
snapshot を PIXEL_SHADER_RESOURCE へ戻して描く。出力中の同じ Texture を SRV として読まない。
描画先・Viewport・Scissor・Descriptor Heap を再設定し、全 UV を有効 texel 範囲へ制限する。

反射RTはRENDER_TARGETとPIXEL_SHADER_RESOURCEを往復し、RTを解除してから読取状態へ移す。
独立したDepth/RTV/DSVを使い、終了時にScene RT/Viewport/Scissorを復元する。
通常Object3dのmapped定数を反射用に上書きせず、反射の各DrawはRoot Constantsへ値をコピーする。
Skyも別インスタンス、草も別の256byte定数バッファを使う。前フレームFence完了という既存前提は保持する。

池は36 float/144 byte、物体反射と遷移は48 float/192 byteのRoot Constantsを使い、
Descriptor Table込みで39/49/49 DWORD。DXC reflectionのoffsetと照合する。
解放は既存 PostDraw の Fence 完了後の Scene 交換時、または通常終了時に行う。
非 copyable Renderer が保持する COM resource と SRV を解放する。

1280x720 RGBA8 snapshotの元レベルは約3.52MiB。反射の640x360 RGBA8 ColorとD24S8 Depthは合計約1.76MiB追加。
実際のallocation alignment・Descriptor Heap・追加Sky/定数/モデルの分はこの値に含めない。
タイトル表示中は毎フレームにCopyResourceとFullscreen Drawが一回。
さらに反射用Sky・景色・可視作物と畑のDraw、草のInstanced Drawが加わる。
草の反射は694 patches x 864 triangles = 599,616 triangles/フレームで、本画面の高詳細モデルは再利用しない。
石岸は主描画とShadowで描く。物体ごとの既存Object3d構成はDrawCallが多く、今後のbatch化候補である。
既存草原も高密度であり、DrawCall 数だけで軽いとは判断しない。GPU 時間・実際の VRAM allocation は未計測。
動的な視点・地形や実農場へ流用する場合は、Depth/反射方式/LOD と負荷を改めて設計する。

## 検証状態

2026-10-15時点で、池の361点座標往復・221クリック・不正導水路設定、配置と法線、草と農道の非重複、
従来の画面遷移、音設定、600秒のタイトル状態、Debug/Release x64 Buildを再確認した。
8本の関連ShaderをDXC SM6.0/-WX/-Zprで検査した。独立したReleaseの画面で
池・昼夜・成長中の作物/木の反射・水路接続・クリック開始と戻り波から進行画面への遷移を確認した。
反射のCPU数式・ソース契約テストはGPUのBarrier/Descriptor実行検証の代用にはしない。
詳細な最終結果はローカルの `.codex/PROJECT_CONTEXT.md` に記録する。

自動 Enter/Space 入力で開始の反応を確認できなかった。CPU の確認入力経路は通るが、
実キーボードの動作と自動入力で反応しない原因は未確認。共有 Input の変更は行っていない。
同一プロセス内のタイトル再入場、GPU 初期化失敗の注入、他 PC/GPU/DPI、Gamepad は未検証。
現在の Release に農場からタイトルへ戻る操作はなく、再起動と同一プロセス再入場を混同しない。
画像と完全に同じアート品質や、一般的な3D水面へ対応した実装とは扱わない。
