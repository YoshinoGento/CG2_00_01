# 水路農業 波紋遷移

## 範囲
対象はTITLEからGAMEPLAYの導入画面、種屋と収穫物展示への出入り。
通常メニューや商品の選択・個数変更・購入確認には待ち時間を追加しない。
「はじめる」周辺のクリック、SPACE/ENTER、Gamepad Aを維持し、クリック案内を追加した。
点滅で非表示の間もHit Rectは有効。画面のどこでも開始する仕様にはしていない。

## 見え方
タイトル・展示の波紋では、クリックした正規化座標が波紋の中心。背景クリックは波紋のみで開始しない。
設定ボタンと開いている設定パネルはクリックを消費し、波紋を発生させない。
SPACE/ENTER/Aは従来の水源付近(0.46,0.67)を使う。ロゴ内の自動波紋とは別。
水滴落下0.18秒 -> 着水波紋0.55秒 -> Cover 0.35秒 -> 不透明な画面を1フレーム描画
-> Scene交換/Initialize -> 反射波のReveal 0.9秒。読み込み待ちを含む総時間は保証しない。
Revealは外周からクリック位置へ戻る。波の前方（中心側）は水色の覆いを残し、
通過した後方（外側）からセレクト画面を表示する。屈折・局所ぼかしも同じ半径を使い、
3つの反射波が内向きに進む。開始・終了で波の振幅を滑らかに0へ落とす。
1滴から38px間隔で3つの波が広がり、後続ほど弱くなる。波の傾きで背景画像のUVを動かし、
波の付近だけ5サンプルを混ぜてぼかす。水面らしい解析的近似であり流体計算ではない。
プレビューは最大4滴、寿命2.4秒。満杯時は新規クリックを捨て、既存波紋を切らない。
開始時はプレビューを消し開始位置の1滴に統一。Reveal時も前Sceneの波紋を消す。
距離は1280x720のvirtual pixelsで計算し、アスペクト比による円の変形を防ぐ。
四隅までの最遠距離+余白から最大半径を求め、端点は完全不透明/透明を明示する。

## 種屋の短い遷移（2026-10-13）
- 入店/退出だけShopEnter/ShopExitを使う。Cover 0.18秒 -> 不透明な画面を描画 ->
  Controllerの表示切替 -> Reveal 0.26秒。描画待ちを除く指定時間は合計0.44秒。
- 入店は畑の画像1.0->1.045倍、店の画像0.945->1.0倍。退出はこの逆。
  Systemがsmoothstepした進行率を渡し、Shaderが画面中央を基準に全画像を動かす。
  3D看板と2D商品画像/文字を同じ変換で動かすので、固定投影の対応が崩れない。
  実Cameraの位置移動や、畑から実空間の店舗まで歩く演出ではない。
- 縮小時に露出する外周は暗転色で埋め、端の画像を引き伸ばさない。
  Cover終了/Reveal開始は完全不透明、Reveal終了は等倍/透明。完了後のHit Rectは従来通り。
- 商品選択/数量/購入確認/キャンセルには遷移を入れない。タイトル・展示・帳簿の波紋は維持。
  水滴音を短い遷移に混ぜず、既存の種袋音/帰路音だけを一度再生する。

## 責務・処理順
- SceneTransitionSystem: 固定容量4滴の位置/寿命、演出style/時間/画像変換、Cover/Reveal/入力解放待ち、有限delta/最大0.25秒、再要求拒否、描画完了通知の状態管理。
- RippleTransitionRenderer: retained snapshot Texture/SRV/PSO/Root Signatureとfullscreen triangle。SystemのFrameだけを描く。
- SceneManager: 次Scene所有、System/Rendererの保持、既存BeginFrameの資源解放・交換境界を統括。
- BaseScene: 任意のpresentation-only callback。通常のUpdate/FixedUpdateは遷移中に呼ばない。
- TitleAudioSettingsSystem: 設定と開始入力の既存分類に、有効な背景クリック通知を追加。Sceneは正規化して転送する。
- TitleScene: 開始/プレビュー通知、遷移中はタイトルの見た目と専用音声だけ更新。UIは状態変更を行わない。
- GamePlayScene: 初期化時にRuntimeController::Refreshで導入表示を準備。
  PrepareRenderFrame(0)でカメラ・照明・HUD・Objectの描画データを用意し、
  ParticleManager::Update(camera,0)で初回描画のカメラを設定する。入力やFixedUpdateは呼ばない。
- 同一Scene内の表示遷移: SceneManager::BeginScreenTransitionは現在のownerを照合し、
  Started/Unavailable/Busyを返す。Unavailableだけ従来の即時表示へfallbackする。
  SystemはScreenPresentation対象の覆いが描画済みの場合だけConsumeScreenChangeを一度返す。
  BeginFrameでGamePlayScene::OnScreenTransitionCoveredを呼び、Controllerの保留Requestを
  取り出して先に消去し、既存の条件を再検証して実行する。その後Refreshと描画準備を行いRevealへ。
  Scene交換・Finalize・Scene Texture解放は行わない。Requestは1件固定で同時要求は受け付けない。
  展示から収穫一覧・大会・Tabへの移動も対象。確認のキャンセルはその場で即時反応する。
  マウス入力が発生したフレームだけvirtual座標を正規化し、キーボードは既定中心を使う。
- ParticleManager: カメラはSceneから借用。未設定ならDrawを行わず、SceneのFinalizeで借用を解除する。
- Game: 初期化時に遷移サービスを接続。進行ロジックは増やさない。

Cover完了の時間だけでは交換しない。Draw後のNotifyCoverPresentedを受け、次のBeginFrameで交換する。
実GPU完了は従来のPostDraw Fence待ちに依存する。新たな別Fenceは追加しない。
Reveal後にLeft/Right mouse/SPACE/ENTER/ESC/A/Bすべてが離れるまで入力を止め、解放検知フレームも
次SceneのUpdateを呼ばない。押し続けた開始入力で導入まで飛ばすことを防ぐ。

## 遷移音（2026-10-13 試作）
- タイトル開始: 着水0.18秒で短い水滴音、Cover開始0.73秒で水の広がり、描画済みCover後のReveal開始で戻る水音。
- 種屋への入場: 種袋を連想する紙の擦れ。収穫物展示への入場: 木台の軽い打音。
  畑へ戻る: 控えめな擦れ。展示から一覧・大会・Tab: 紙をめくる音。
- SceneTransitionSystemがphase境界のcue bitmaskを一度だけ通知。Controllerが行き先themeを指定し、
  SceneManagerが専用TransitionAudioSystemへ転送する。UIは音や遷移状態を変更しない。
- AudioSystemはFramework Audioを借用し、Scene交換を越えて保持する。起動時に7 WAVを読む。
  同時Voiceは最大4、満杯時は既存音を切らず新規cueを捨てる。終了済みhandleを再利用する。
  終了時は専用Voiceを止めてから専用Clipを解放。他Systemの音源・Voiceへ触らない。
- 元音量peak約0.52、再生gain0.30の小さな試作音。環境音/水音スライダーとは独立で、SE用UIはまだない。
  波紋プレビュー、商品選択、数量変更、購入確認などには今回SEを追加していない。
- 7個合計276480 PCM bytes（WAV headers別）。Audio backendのforward/reverse保持は552960 bytes。
  読込は起動時だけだがPlay時のXAudio2 SourceVoice生成とmapの確保は既存backendに残る。
  Runtime失敗は警告/無音で継続。PCM pointer寿命を先に切らない。DX12資源経路は変更なし。
- tools/audio/generate_transition_audio.pyによる固定seedのオリジナル合成。外部音源を使用せず、
  実録音/物理シミュレーションとは区別する。既存環境音の利用条件は別のまま。
  Audio全体のglobal temporal設定は既存仕様を継承する。遷移専用busへの分離は未実装。
- 種屋の画面演出は上記の短い暗転へ分離。展示/帳簿はまだ波紋で、専用の画面演出は未実装。

## GPU・限界
波紋または遷移が有効なフレームだけ1 CopyResourceと1 fullscreen DrawCall。
Root Constants 32 floatsとDescriptor Table 1 DWORD（合計33、上限64）を使用。
追加Upload CB/VB/IBはない。短い遷移は有効UV内で1回サンプルし、完全暗転はサンプルしない。
これはShaderの処理経路であり、実測の高速化を示すものではない。
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
2026-10-13の独立Release録画で水滴・屈折・内向き反射波と外側からのセレクト表示を確認。
背景クリック/開始クリックと次画面の選択待機も確認。旧円形wipeの実機記録とは区別する。
CPU状態テスト、タイトル/クリック/設定回帰、Shader vs/ps_6_0 -WX、Debug/Releaseビルドは成功。
Releaseでクリック開始・外向きの波・不透明な待機・導入画面のRevealを確認した。
初回描画時の未設定カメラ参照を実機で再現し、上記のゼロ時間準備で修正。
種屋の最大化で入退店とHit Rectは確認済み。exclusive fullscreen・別GPU/DPI・
実Gamepad・押し続けた入力の実機確認は未完了。畑の既存の白い帯は別課題。
検証の詳細は[検証記録](../testing/SceneTransition_20261007.md)を参照。

## 参考
解析的な波と傾きの考え方は[NVIDIA GPU Gems: Effective Water Simulation](https://developer.nvidia.com/gpugems/gpugems/part-i-natural-effects/chapter-1-effective-water-simulation-physical-models)を参考にした。
本実装はscreen-space表現で、同記事の3D水面実装そのものではない。
状態遷移は[Microsoft: Resource Barriers](https://learn.microsoft.com/en-us/windows/win32/direct3d12/using-resource-barriers-to-synchronize-resource-states-in-direct3d-12)のread/write分離に従う。
