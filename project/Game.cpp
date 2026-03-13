#include "Game.h"
#include "SceneManager.h"
#include "SceneFactory.h"
#include "DirectXCommon.h"
#include "SrvManager.h"
#include "ImGuiManager.h"

Game::Game() = default;
Game::~Game() = default;

void Game::Initialize() {
	Framework::Initialize();
	sceneFactory_ = std::make_unique<SceneFactory>();
	SceneManager::GetInstance()->SetSceneFactory(sceneFactory_.get());
	SceneManager::GetInstance()->ChangeScene("TITLE");
}

/**
 * 更新：ImGuiの開始・終了をここで一括管理します
 */
void Game::Update() {
	Framework::Update();

	// ★重要：ここで ImGui の受付を開始する
	ImGuiManager::GetInstance()->Begin();

	// シーンの更新（この中で ImGui::Begin を呼ぶ必要がなくなります）
	SceneManager::GetInstance()->Update();

	// ★重要：ここで ImGui の受付を終了する
	ImGuiManager::GetInstance()->End();
}

void Game::Draw() {
	dxCommon_->PreDraw();
	srvManager_->PreDraw();

	// シーンの描画
	SceneManager::GetInstance()->Draw();

	// ★重要：ImGuiの最終描画（UpdateでのBegin/Endペアが確定しているので安全）
	ImGuiManager::GetInstance()->Draw();

	dxCommon_->PostDraw();
}

void Game::Finalize() {
	SceneManager::DeleteInstance();
	Framework::Finalize();
}