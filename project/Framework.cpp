#include "Framework.h"

// --- ここですべてのヘッダーを読み込むことで unique_ptr が正常に動くようにする ---
#include "WinApp.h"
#include "DirectXCommon.h"
#include "Input.h"
#include "Audio.h"
#include "SrvManager.h"
#include "ImGuiManager.h"
#include "SpriteCommon.h"
#include "Object3dCommon.h"
#include "ModelManager.h"
#include "ParticleManager.h"

/**
 * コンストラクタとデストラクタの実装
 * ここですべての型情報が揃っているため、安全に生成・破棄ができます。
 */
Framework::Framework() = default;
Framework::~Framework() = default;

/**
 * ゲームの実行順序を制御
 */
void Framework::Run() {
	Initialize(); // 1. 準備

	while (true) {
		Update(); // 2. 更新
		if (IsEndRequest()) break;
		Draw();   // 3. 描画
	}

	Finalize();   // 4. 後片付け
}

void Framework::Initialize() {
	winApp_ = std::make_unique<WinApp>();
	winApp_->Initialize();

	dxCommon_ = std::make_unique<DirectXCommon>();
	dxCommon_->Initialize(winApp_.get());

	srvManager_ = std::make_unique<SrvManager>();
	srvManager_->Initialize(dxCommon_.get());

	input_ = std::make_unique<Input>();
	input_->Initialize(winApp_.get());

	audio_ = std::make_unique<Audio>();
	audio_->Initialize();

	spriteCommon_ = std::make_unique<SpriteCommon>();
	spriteCommon_->Initialize(dxCommon_.get(), srvManager_.get());

	object3dCommon_ = std::make_unique<Object3dCommon>();
	object3dCommon_->Initialize(dxCommon_.get(), srvManager_.get());

	modelManager_ = std::make_unique<ModelManager>();
	modelManager_->Initialize(dxCommon_.get(), srvManager_.get());

	particleManager_ = std::make_unique<ParticleManager>();
	particleManager_->Initialize(dxCommon_.get(), srvManager_.get());

	// ImGuiのセットアップ
	ImGuiManager::GetInstance()->Initialize(winApp_.get(), dxCommon_.get(), srvManager_.get());
}

void Framework::Finalize() {
	ImGuiManager::GetInstance()->Finalize();
	audio_->Finalize();
	winApp_->Finalize();
}

void Framework::Update() {
	if (winApp_->ProcessMessage()) {
		endRequest_ = true;
	}
	input_->Update();
}