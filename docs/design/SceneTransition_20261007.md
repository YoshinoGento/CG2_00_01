# 水路農業 波紋遷移

## 範囲
最初の対象はTITLEからGAMEPLAYの導入画面。ショップ・展示・メニューの切り替えは未対応。
「はじめる」周辺のクリック、SPACE/ENTER、Gamepad Aを維持し、クリック案内を追加した。
点滅で非表示の間もHit Rectは有効。画面のどこでも開始する仕様にはしていない。

## 見え方
画面正規化座標(0.46,0.67)、タイトルの水源付近から円形に広がる。
Cover 0.35秒 -> 不透明な画面を1フレーム描画 -> Scene交換/Initialize -> Reveal 0.35秒。
Scene交換後も同じ中心から外向きに開く。固定中心は開始演出の視線誘導のためで、
ロゴ内の雨のランダム中心とは別。読み込みが遅ければ覆ったまま待つので、総時間は保証しない。
深い水色で画面を覆い、境界に主波と22px後ろの弱い波を描く。画面全体のUV歪曲は行わない。
距離は1280x720のvirtual pixelsで計算し、アスペクト比による円の変形を防ぐ。
四隅までの最遠距離+余白から最大半径を求め、端点は完全不透明/透明を明示する。

## 責務・処理順
- SceneTransitionSystem: Cover/Reveal/入力解放待ち、有限delta/最大0.25秒、再要求拒否、描画完了通知の状態管理。
- RippleTransitionRenderer: retained PSO/Root Signatureとfullscreen triangle。SystemのFrameだけを描く。
- SceneManager: 次Scene所有、System/Rendererの保持、既存BeginFrameの資源解放・交換境界を統括。
- BaseScene: 任意のpresentation-only callback。通常のUpdate/FixedUpdateは遷移中に呼ばない。
- TitleScene: 開始通知、遷移中はタイトルの見た目と専用音声だけ更新。UIは状態変更を行わない。
- GamePlayScene: 初期化時に既存RuntimeController::Refreshで導入表示を準備し、操作なしでReveal可能にする。
- Game: 初期化時に遷移サービスを接続。進行ロジックは増やさない。

Cover完了の時間だけでは交換しない。Draw後のNotifyCoverPresentedを受け、次のBeginFrameで交換する。
実GPU完了は従来のPostDraw Fence待ちに依存する。新たな別Fenceは追加しない。
Reveal後にLeft mouse/SPACE/ENTER/Aすべてが離れるまで入力を止め、解放検知フレームも
次SceneのUpdateを呼ばない。押し続けた開始入力で導入まで飛ばすことを防ぐ。

## GPU・限界
遷移中だけ1 DrawCall。Root Constants 4 floatsをcommand streamにコピーし、
Upload CB/VB/IB、SRV、RenderTarget、texture、Barrierは追加しない。
RendererはSceneより長く存在し、Scene Texture解放の影響を受けない。
2Sceneの同時描画や毎フレームの動的確保は行わない。実GPU時間は未測定。
Shader初期化に失敗した場合は警告を出し、直接切り替えを残す。障害時の演出品質は保証しない。
現在のPostEffect/最終表示の前に合成するので、将来の強い画面効果との重ね合わせは再検証が必要。

## 検証
CPU状態テスト、タイトル/クリック/設定回帰、Shader vs/ps_6_0 -WX、Debug/Releaseビルドは成功。
実画面・遷移速度・最大化・別GPU/DPI・実Gamepadの確認状況は作業記録と個別検証記録を参照。
