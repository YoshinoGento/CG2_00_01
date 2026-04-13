#include "Game.h"
#include "SceneManager.h"
#include "SceneFactory.h"
#include "DirectXCommon.h"
#include "SrvManager.h"
#include "ImGuiManager.h"
#include "GamePlayScene.h" 
#include "Audio.h" // ★必須
#include <cmath>

// 静的メンバの初期化
Vector2 Game::mousePosInViewport_ = { 0, 0 };

Game::Game() = default;
Game::~Game() = default;

void Game::Initialize() {
	Framework::Initialize();

	viewportSrvIndex_ = srvManager_->Allocate();
	srvManager_->CreateSRVforTexture2D(
		viewportSrvIndex_,
		dxCommon_->GetRenderTextureResource(),
		DXGI_FORMAT_R8G8B8A8_UNORM,
		1
	);

	sceneFactory_ = std::make_unique<SceneFactory>();
	SceneManager::GetInstance()->SetSceneFactory(sceneFactory_.get());
	SceneManager::GetInstance()->ChangeScene("TITLE");
}

void Game::Finalize() {
	SceneManager::DeleteInstance();
	Framework::Finalize();
}

void Game::Update() {
	Framework::Update();

	ImGuiManager::GetInstance()->Begin();

	// 1. 全画面 DockSpace
	ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

	// 2. 「Game Viewport」ウィンドウ
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	if (ImGui::Begin("Game Viewport")) {
		ImVec2 contentSize = ImGui::GetContentRegionAvail();
		float targetAspect = 1280.0f / 720.0f;
		ImVec2 displaySize = contentSize;
		if (displaySize.x / displaySize.y > targetAspect) {
			displaySize.x = displaySize.y * targetAspect;
		} else {
			displaySize.y = displaySize.x / targetAspect;
		}

		ImVec2 offset = { (contentSize.x - displaySize.x) * 0.5f, (contentSize.y - displaySize.y) * 0.5f };
		ImVec2 currentPos = ImGui::GetCursorPos();
		ImGui::SetCursorPos({ currentPos.x + offset.x, currentPos.y + offset.y });

		ImVec2 mousePos = ImGui::GetIO().MousePos;
		ImVec2 imageTopLeft = ImGui::GetCursorScreenPos();
		mousePosInViewport_.x = (mousePos.x - imageTopLeft.x) / displaySize.x * 1280.0f;
		mousePosInViewport_.y = (mousePos.y - imageTopLeft.y) / displaySize.y * 720.0f;

		ImGui::Image((ImTextureID)srvManager_->GetGPUDescriptorHandle(viewportSrvIndex_).ptr, displaySize);
	}
	ImGui::End();
	ImGui::PopStyleVar();

	// 3. --- エディタ用ウィンドウ ---
	BaseScene* current = SceneManager::GetInstance()->GetCurrentScene();
	GamePlayScene* playScene = dynamic_cast<GamePlayScene*>(current);

	if (playScene) {
		// 全般設定
		ImGui::Begin("Global Settings");
		const char* targets[] = { "None", "Sprite", "Object3D", "Particle", "Sphere" };
		ImGui::Combo("Edit Focus", &playScene->selectedTarget_, targets, 5);
		ImGui::End();

		// オブジェクトエディタ
		ImGui::Begin("Object Editor");

		// ★描画優先の設定（modelPriority_ が必要）
		if (ImGui::CollapsingHeader("Render Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
			const char* drawOrders[] = { "3D -> 2D (2D on Top)", "2D -> 3D (3D on Top)" };
			ImGui::Combo("Draw Priority", &playScene->modelPriority_, drawOrders, 2);
		}

		if (ImGui::CollapsingHeader("Sprite (2D)")) {
			ImGui::DragFloat2("Position", &playScene->spritePos_.x, 1.0f);
		}

		if (ImGui::CollapsingHeader("Sphere (3D)", ImGuiTreeNodeFlags_DefaultOpen)) {
			if (ImGui::SliderFloat("Radius", &playScene->sphereRadius_, 0.1f, 10.0f)) {
				playScene->CreateSphere(playScene->sphereRadius_);
			}
			ImGui::DragFloat3("Position", &playScene->spherePos_.x, 0.1f);

			// ★回転の設定（objectRot_ が必要）
			// &playScene->objectRot_.x を float* として渡します
			ImGui::DragFloat3("Rotation", &playScene->objectRot_.x, 0.01f);

			if (playScene->sphereObj_) {
				float shiny = playScene->sphereObj_->GetShininess();
				if (ImGui::SliderFloat("Shininess", &shiny, 1.0f, 200.0f)) {
					playScene->sphereObj_->SetShininess(shiny);
				}
			}
		}

		if (ImGui::CollapsingHeader("Particle")) {
			ImGui::DragFloat3("Emit Pos", playScene->particleEmitPos_, 0.1f);
			ImGui::SliderInt("Count", &playScene->particleEmitCount_, 1, 100);
		}
		ImGui::End();

		// オーディオ操作
		ImGui::Begin("Audio Control");
		const char* tracks[] = { "Track 1", "Track 2" };
		ImGui::Combo("BGM Select", &playScene->currentBgmIndex_, tracks, 2);
		if (ImGui::Button("PLAY")) GetAudio()->PlayWave(playScene->soundHandles_[playScene->currentBgmIndex_], playScene->isBgmLoop_);
		ImGui::SameLine();
		if (ImGui::Button("STOP")) GetAudio()->StopWave(playScene->soundHandles_[playScene->currentBgmIndex_]);
		ImGui::Checkbox("Loop", &playScene->isBgmLoop_);
		ImGui::End();

		// システムモニター
		ImGui::Begin("System Monitor");
		ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
		if (ImGui::Button("Clear Logs")) playScene->debugLogs_.clear();
		ImGui::BeginChild("Logs", { 0, 100 }, true);
		for (const auto& log : playScene->debugLogs_) ImGui::TextUnformatted(log.c_str());
		if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.0f);
		ImGui::EndChild();
		ImGui::End();
	}

	SceneManager::GetInstance()->Update();
	ImGuiManager::GetInstance()->End();
}

void Game::Draw() {
	dxCommon_->PreDraw();
	srvManager_->PreDraw();
	SceneManager::GetInstance()->Draw();

	dxCommon_->PreDrawToSwapChain();
	ImGuiManager::GetInstance()->Draw();

	dxCommon_->PostDraw();
}