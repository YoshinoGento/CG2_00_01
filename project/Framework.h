#pragma once
#include <memory>

// --- 前方宣言（ビルドを速くするための「名前だけの登録」） ---
class WinApp;
class DirectXCommon;
class Input;
class Audio;
class SrvManager;
class SpriteCommon;
class Object3dCommon;
class ModelManager;
class ParticleManager;

/**
 * Frameworkクラス
 * ゲームの実行手順（初期化・ループ・終了）を管理する基底クラスです。
 */
class Framework {
public:
	// コンストラクタ
	Framework();

	// デストラクタ（仮想関数）
	// unique_ptrのエラーを避けるため、実装（中身）は .cpp で書きます。
	virtual ~Framework();

	// ゲームのメイン処理を実行する関数
	void Run();

	// 基本的な初期化
	virtual void Initialize();

	// 終了処理
	virtual void Finalize();

	// 更新
	virtual void Update();

	// 描画（各ゲームで実装する純粋仮想関数）
	virtual void Draw() = 0;

	// 終了リクエストの確認
	virtual bool IsEndRequest() { return endRequest_; }

protected:
	// マネージャ群（スマートポインタで安全に管理）
	std::unique_ptr<WinApp> winApp_;
	std::unique_ptr<DirectXCommon> dxCommon_;
	std::unique_ptr<SrvManager> srvManager_;
	std::unique_ptr<Input> input_;
	std::unique_ptr<Audio> audio_;

	std::unique_ptr<SpriteCommon> spriteCommon_;
	std::unique_ptr<Object3dCommon> object3dCommon_;
	std::unique_ptr<ModelManager> modelManager_;
	std::unique_ptr<ParticleManager> particleManager_;

	// 終了フラグ
	bool endRequest_ = false;
};