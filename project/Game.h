#pragma once
#include "WinApp.h"
#include "DirectXCommon.h"
#include "SpriteCommon.h"
#include "Sprite.h"
#include "Object3dCommon.h"
#include "Object3d.h"
#include "ModelManager.h"
#include "Model.h"
#include "Input.h"
#include "Audio.h" 
#include "ImGuiManager.h"
#include "ParticleManager.h"
#include "Camera.h"
#include <memory>
#include <vector>
#include <string>

/**
 * ゲーム全体の管理クラス
 * WinMainから呼び出され、初期化から終了までの全処理を担当します。
 */
class Game {
public:
	// --- 基本メソッド ---

	// 初期化
	void Initialize();

	// 終了処理
	void Finalize();

	// 更新
	void Update();

	// 描画
	void Draw();

	// 終了リクエスト（ウィンドウが閉じられたか）の確認
	bool IsEndRequest() const { return isEndRequest_; }

private:
	// --- システム・基盤コンポーネント ---
	std::unique_ptr<WinApp> winApp_;
	std::unique_ptr<DirectXCommon> dxCommon_;
	std::unique_ptr<SrvManager> srvManager_;
	std::unique_ptr<Input> input_;
	std::unique_ptr<Audio> audio_;

	// --- 描画系マネージャ ---
	std::unique_ptr<SpriteCommon> spriteCommon_;
	std::unique_ptr<Object3dCommon> object3dCommon_;
	std::unique_ptr<ModelManager> modelManager_;
	std::unique_ptr<ParticleManager> particleManager_;

	// --- ゲームオブジェクト・リソース ---
	std::unique_ptr<Sprite> sprite_;
	std::unique_ptr<Object3d> object3d_;
	std::unique_ptr<Camera> camera_;

	// オーディオ関連
	uint32_t soundHandles_[2];
	int currentBgmIndex_ = 0;
	bool isBgmLoop_ = true;

	// スプライト関連
	uint32_t texMonsterHandle_ = 0;
	uint32_t texWhiteHandle_ = 0;
	Vector2 spritePos_ = { 640.0f, 360.0f };

	// 描画優先度・パーティクル設定
	bool spriteOnTopVsParticle_ = true;
	int modelPriority_ = 0; // 0: Sprite Front, 1: Model Front
	int selectedParticleType_ = 0;

	// 内部フラグ
	bool isEndRequest_ = false;
};