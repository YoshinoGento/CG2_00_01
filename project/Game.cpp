#include "Game.h"
#include "SceneManager.h"
#include "SceneFactory.h"
#include "DirectXCommon.h"
#include "SrvManager.h"
#include "ImGuiManager.h"
#include "GamePlayScene.h" 
#include "Audio.h"
#include "Object3d.h"
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
	ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

	// --- ビューポート表示 ---
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	if (ImGui::Begin("Game Viewport")) {
		ImVec2 contentSize = ImGui::GetContentRegionAvail();
		float targetAspect = 1280.0f / 720.0f;
		ImVec2 displaySize = contentSize;
		if (displaySize.x / displaySize.y > targetAspect) displaySize.x = displaySize.y * targetAspect;
		else displaySize.y = displaySize.x / targetAspect;

		ImVec2 offset = { (contentSize.x - displaySize.x) * 0.5f, (contentSize.y - displaySize.y) * 0.5f };
		ImGui::SetCursorPos({ ImGui::GetCursorPos().x + offset.x, ImGui::GetCursorPos().y + offset.y });

		ImVec2 mousePos = ImGui::GetIO().MousePos;
		ImVec2 imageTopLeft = ImGui::GetCursorScreenPos();
		mousePosInViewport_.x = (mousePos.x - imageTopLeft.x) / displaySize.x * 1280.0f;
		mousePosInViewport_.y = (mousePos.y - imageTopLeft.y) / displaySize.y * 720.0f;

		ImGui::Image((ImTextureID)srvManager_->GetGPUDescriptorHandle(viewportSrvIndex_).ptr, displaySize);
	}
	ImGui::End();
	ImGui::PopStyleVar();

	// --- エディタ UI ---
	BaseScene* current = SceneManager::GetInstance()->GetCurrentScene();
	GamePlayScene* playScene = dynamic_cast<GamePlayScene*>(current);

	if (playScene) {
		// 全般設定
		ImGui::Begin("Global Settings");
		const char* targets[] = { "None", "Sprite", "Object3D", "Particle", "Sphere" };
		ImGui::Combo("Edit Focus", &playScene->selectedTarget_, targets, 5);
		ImGui::End();

		// 表示・非表示切り替えとカリング設定
		ImGui::Begin("Visibility & Cull");
		ImGui::Checkbox("Terrain", &playScene->showTerrain_);
		ImGui::Checkbox("Sphere", &playScene->showSphere_);
		ImGui::Checkbox("Plane", &playScene->showPlane_);
		// ★追加：スプライトの表示切り替え
		ImGui::Checkbox("Sprite (2D)", &playScene->showSprite_);
		ImGui::Checkbox("Particles", &playScene->showParticles_);

		ImGui::Separator();

		const char* cullItems[] = { "None (両面描画)", "Front (前面削除)", "Back (背面削除)" };
		ImGui::Combo("Cull Mode", &playScene->cullMode_, cullItems, 3);
		ImGui::End();

		// カメラ操作
		ImGui::Begin("Camera Control");
		ImGui::DragFloat3("Camera Pos", &playScene->cameraPos_.x, 0.1f);
		ImGui::DragFloat3("Camera Rot", &playScene->cameraRot_.x, 0.01f);
		ImGui::End();

		// オブジェクトエディタ
		ImGui::Begin("Object Editor");
		if (ImGui::CollapsingHeader("Lighting", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::DragFloat3("Direction", &playScene->lightDirection_.x, 0.01f, -1.0f, 1.0f);
			ImGui::ColorEdit3("Color", &playScene->lightColor_.x);
			ImGui::SliderFloat("Intensity", &playScene->lightIntensity_, 0.0f, 10.0f);
		}
		if (ImGui::CollapsingHeader("Sphere (3D)")) {
			if (ImGui::SliderFloat("Radius", &playScene->sphereRadius_, 0.1f, 10.0f)) playScene->CreateSphere(playScene->sphereRadius_);
			ImGui::DragFloat3("Position", &playScene->spherePos_.x, 0.1f);
			ImGui::DragFloat3("Rotation", &playScene->objectRot_.x, 0.01f);
		}
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