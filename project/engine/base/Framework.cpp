#include "Framework.h"
#include "base/WinApp.h"
#include "base/DirectXCommon.h"
#include "io/Input.h"
#include "audio/Audio.h"
#include "base/SrvManager.h"
#include "base/ImGuiManager.h"
#include "2d/SpriteCommon.h"
#include "3d/Object3dCommon.h"
#include "3d/ModelManager.h"
#include "effect/ParticleManager.h"

// 静的変数の実体を定義（最初は空っぽ）
Framework* Framework::instance = nullptr;

/**
 * どこからでも自分自身を取得できる関数
 */
Framework* Framework::GetInstance() {
	return instance;
}

/**
 * コンストラクタ（生成時）
 */
Framework::Framework() {
	// 自分が作られたとき、その場所（this）を instance に保存する
	instance = this;
}

Framework::~Framework() {
	instance = nullptr;
}

/**
 * メインループの制御
 */
void Framework::Run() {
	Initialize(); // 準備

	while (true) {
		Update(); // 更新
		if (IsEndRequest()) break;
		Draw();   // 描画
	}

	Finalize(); // 終了
}

/**
 * エンジンとしての基本初期化
 */
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

	// ImGuiの準備
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