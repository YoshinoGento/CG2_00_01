# 水路農業 波紋遷移

## 範囲
最初の対象はTITLEからGAMEPLAYの導入画面。ショップ・展示・メニューの切り替えは未対応。
「はじめる」周辺のクリック、SPACE/ENTER、Gamepad Aを維持し、クリック案内を追加した。
点滅で非表示の間もHit Rectは有効。画面のどこでも開始する仕様にはしていない。

## 見え方
クリックした正規化座標が波紋の中心。背景クリックは波紋のみで開始しない。
設定ボタンと開いている設定パネルはクリックを消費し、波紋を発生させない。
SPACE/ENTER/Aは従来の水源付近(0.46,0.67)を使う。ロゴ内の自動波紋とは別。
水滴落下0.18秒 -> 着水波紋0.55秒 -> Cover 0.35秒 -> 不透明な画面を1フレーム描画
-> Scene交換/Initialize -> Reveal 0.35秒。読み込み待ちを含む総時間は保証しない。
1滴から38px間隔で3つの波が広がり、後続ほど弱くなる。波の傾きで背景画像のUVを動かし、
波の付近だけ5サンプルを混ぜてぼかす。水面らしい解析的近似であり流体計算ではない。
プレビューは最大4滴、寿命2.4秒。満杯時は新規クリックを捨て、既存波紋を切らない。
開始時はプレビューを消し開始位置の1滴に統一。Reveal時も前Sceneの波紋を消す。
距離は1280x720のvirtual pixelsで計算し、アスペクト比による円の変形を防ぐ。
四隅までの最遠距離+余白から最大半径を求め、端点は完全不透明/透明を明示する。

## 責務・処理順
- SceneTransitionSystem: 固定容量4滴の位置/寿命、Cover/Reveal/入力解放待ち、有限delta/最大0.25秒、再要求拒否、描画完了通知の状態管理。
- RippleTransitionRenderer: retained snapshot Texture/SRV/PSO/Root Signatureとfullscreen triangle。SystemのFrameだけを描く。
- SceneManager: 次Scene所有、System/Rendererの保持、既存BeginFrameの資源解放・交換境界を統括。
- BaseScene: 任意のpresentation-only callback。通常のUpdate/FixedUpdateは遷移中に呼ばない。
- TitleAudioSettingsSystem: 設定と開始入力の既存分類に、有効な背景クリック通知を追加。Sceneは正規化して転送する。
- TitleScene: 開始/プレビュー通知、遷移中はタイトルの見た目と専用音声だけ更新。UIは状態変更を行わない。
- GamePlayScene: 初期化時にRuntimeController::Refreshで導入表示を準備。
  PrepareRenderFrame(0)でカメラ・照明・HUD・Objectの描画データを用意し、
  ParticleManager::Update(camera,0)で初回描画のカメラを設定する。入力やFixedUpdateは呼ばない。
- ParticleManager: カメラはSceneから借用。未設定ならDrawを行わず、SceneのFinalizeで借用を解除する。
- Game: 初期化時に遷移サービスを接続。進行ロジックは増やさない。

Cover完了の時間だけでは交換しない。Draw後のNotifyCoverPresentedを受け、次のBeginFrameで交換する。
実GPU完了は従来のPostDraw Fence待ちに依存する。新たな別Fenceは追加しない。
Reveal後にLeft mouse/SPACE/ENTER/Aすべてが離れるまで入力を止め、解放検知フレームも
次SceneのUpdateを呼ばない。押し続けた開始入力で導入まで飛ばすことを防ぐ。

## GPU・限界
波紋または遷移が有効なフレームだけ1 CopyResourceと1 fullscreen DrawCall。
Root Constants 28 floatsをcommand streamへコピー。追加Upload CB/VB/IBはない。
Rendererが独立したDEFAULT TextureとSRV 1個を保持する。Scene Texture解放の対象ではない。
Scene RTをunbindしてCOPY_SOURCEへ、snapshotをCOPY_DESTへ遷移しコピー。
SceneをRENDER_TARGETへ戻し、snapshotをPIXEL_SHADER_RESOURCEへ遷移してScene RTを再bindし描画。
出力RTそのものをサンプルしない。次フレームはsnapshotをCOPY_DESTへ戻して再利用する。
解放は既存PostDraw Fence完了後、SrvManagerより先。copyは禁止しDescriptor二重解放を防ぐ。
Scene RTは固定1280x720、swapchain resizeとは独立。snapshotとの寸法/format不一致なら描画を中止。
Texture基礎容量3.52MiBに加えallocation alignmentが必要。実VRAM/GPU時間は未測定。
UVはhalf texel内へclampし、中心での除算と重なる波の最大変位14pxを制限する。
2Sceneの同時描画や毎フレームの動的確保は行わない。実GPU時間は未測定。
Shader初期化に失敗した場合は警告を出し、直接切り替えを残す。障害時の演出品質は保証しない。
現在のPostEffect/最終表示の前に合成するので、将来の強い画面効果との重ね合わせは再検証が必要。

## 検証
水滴・屈折の実機確認は別途追加する。以下は先行した円形wipeの検証記録。
CPU状態テスト、タイトル/クリック/設定回帰、Shader vs/ps_6_0 -WX、Debug/Releaseビルドは成功。
Releaseでクリック開始・外向きの波・不透明な待機・導入画面のRevealを確認した。
初回描画時の未設定カメラ参照を実機で再現し、上記のゼロ時間準備で修正。
最大化・別GPU/DPI・実Gamepad・押し続けた入力の実機確認は未完了。
検証の詳細は[検証記録](../testing/SceneTransition_20261007.md)を参照。

## 参考
解析的な波と傾きの考え方は[NVIDIA GPU Gems: Effective Water Simulation](https://developer.nvidia.com/gpugems/gpugems/part-i-natural-effects/chapter-1-effective-water-simulation-physical-models)を参考にした。
本実装はscreen-space表現で、同記事の3D水面実装そのものではない。
状態遷移は[Microsoft: Resource Barriers](https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-resource-barriers-to-synchronize-resource-states-in-direct3d-12)のread/write分離に従う。
