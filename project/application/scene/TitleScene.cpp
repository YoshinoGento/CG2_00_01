#include "TitleScene.h"
#include "SceneManager.h"
#include "base/Framework.h"
#include "io/Input.h"
#include "base/ImGuiManager.h"

// ★削除: GamePlayScene.h をインクルードする必要はありません
// 文字列だけで遷移できるため、このファイルは他のシーンの中身を知らなくて済みます

void TitleScene::Initialize() {}
void TitleScene::Finalize() {}

void TitleScene::Update() {
	Input* input = Framework::GetInstance()->GetInput();
	if (input->TriggerKey(InputKey::M)) {
		SceneManager::GetInstance()->ChangeScene("MAGNET_PROTOTYPE");
		return;
	}

	// 1. キー入力による遷移
	if (input->TriggerKey(DIK_RETURN)) {
		// ★修正: new ではなく名前を指定して ChangeScene を呼ぶ
		SceneManager::GetInstance()->ChangeScene("GAMEPLAY");
		return;
	}

#ifdef USE_IMGUI
	// 2. ImGuiボタンによる遷移
	ImGui::Begin("TITLE SCREEN");
	ImGui::SetWindowPos({ 540, 300 }, ImGuiCond_Once);
	ImGui::Text("PRESS ENTER TO START");
	ImGui::Text("PRESS M FOR MAGNET PHYSICS PROTOTYPE");

	if (ImGui::Button("START GAME", ImVec2(200, 50))) {
		// ★修正: ここも同様に名前指定
		SceneManager::GetInstance()->ChangeScene("GAMEPLAY");
	}
	if (ImGui::Button("MAGNET PHYSICS PROTOTYPE", ImVec2(260, 50))) {
		SceneManager::GetInstance()->ChangeScene("MAGNET_PROTOTYPE");
	}
	ImGui::End();
#endif
}

void TitleScene::Draw() {}
