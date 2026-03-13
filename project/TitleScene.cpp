#include "TitleScene.h"
#include "GamePlayScene.h"
#include "SceneManager.h"
#include "Framework.h"
#include "Input.h"
#include "ImGuiManager.h"

void TitleScene::Initialize() {}
void TitleScene::Finalize() {}

void TitleScene::Update() {
	Input* input = Framework::GetInstance()->GetInput();

	// シーン遷移の判定
	if (input->TriggerKey(DIK_RETURN)) {
		SceneManager::GetInstance()->SetNextScene(new GamePlayScene());
		return;
	}

	// ★修正：ImGuiManager::Begin/End は Game.cpp が行うので、ここでは中身だけ書く
#ifdef USE_IMGUI
	ImGui::Begin("TITLE SCREEN");
	ImGui::SetWindowPos({ 540, 300 }, ImGuiCond_Once);
	ImGui::Text("PRESS ENTER TO START");

	if (ImGui::Button("START GAME", ImVec2(200, 50))) {
		SceneManager::GetInstance()->SetNextScene(new GamePlayScene());
	}
	ImGui::End();
#endif
}

void TitleScene::Draw() {}