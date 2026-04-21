#include "Game.h"
#include "scene/SceneManager.h"
#include "scene/SceneFactory.h"
#include "base/DirectXCommon.h"
#include "base/SrvManager.h"
#include "base/ImGuiManager.h"
#include "scene/GamePlayScene.h" 
#include "audio/Audio.h"
#include "3d/Object3d.h"
#include <cmath>

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

	// ビューポート表示
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

	BaseScene* current = SceneManager::GetInstance()->GetCurrentScene();
	GamePlayScene* playScene = dynamic_cast<GamePlayScene*>(current);

	if (playScene) {
		ImGui::Begin("Global Settings");
		const char* targets[] = { "None", "Sprite", "Object3D", "Particle", "Sphere" };
		ImGui::Combo("Edit Focus", &playScene->selectedTarget_, targets, 5);
		ImGui::End();

		ImGui::Begin("Visibility & Cull");
		ImGui::Checkbox("Terrain", &playScene->showTerrain_);
		ImGui::Checkbox("Sphere", &playScene->showSphere_);
		ImGui::Checkbox("Plane", &playScene->showPlane_);
		ImGui::Checkbox("Sprite (2D)", &playScene->showSprite_);
		ImGui::Checkbox("Particles", &playScene->showParticles_);
		ImGui::Separator();
		const char* cullItems[] = { "None (両面)", "Front (前面削除)", "Back (背面削除)" };
		ImGui::Combo("Cull Mode", &playScene->cullMode_, cullItems, 3);
		ImGui::End();

		ImGui::Begin("Camera Control");
		ImGui::DragFloat3("Camera Pos", &playScene->cameraPos_.x, 0.1f);
		ImGui::DragFloat3("Camera Rot", &playScene->cameraRot_.x, 0.01f);
		ImGui::End();

		ImGui::Begin("Object Editor");
		// 既存の平行光源
		if (ImGui::CollapsingHeader("Directional Light")) {
			ImGui::DragFloat3("Direction", &playScene->lightDirection_.x, 0.01f, -1.0f, 1.0f);
			// 正規化処理を追加
			if (ImGui::DragFloat3("D-Light Direction", &playScene->lightDirection_.x, 0.01f, -1.0f, 1.0f)) {
				if (MatrixMath::Length(playScene->lightDirection_) > 0.0f) {
					playScene->lightDirection_ = MatrixMath::Normalize(playScene->lightDirection_);
				}
			}
			ImGui::ColorEdit3("Color", &playScene->lightColor_.x);
			ImGui::SliderFloat("Intensity", &playScene->lightIntensity_, 0.0f, 10.0f);
		}

		// ★追加：スポットライト
		if (ImGui::CollapsingHeader("Spot Light", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::ColorEdit3("S-Light Color", &playScene->spotLightColor_.x);
			ImGui::DragFloat3("S-Light Position", &playScene->spotLightPos_.x, 0.1f);
			ImGui::SliderFloat("S-Light Intensity", &playScene->spotLightIntensity_, 0.0f, 20.0f);
			// 正規化処理を追加
			if (ImGui::DragFloat3("S-Light Direction", &playScene->spotLightDir_.x, 0.01f, -1.0f, 1.0f)) {
				if (MatrixMath::Length(playScene->spotLightDir_) > 0.0f) {
					playScene->spotLightDir_ = MatrixMath::Normalize(playScene->spotLightDir_);
				}
			}
			ImGui::SliderFloat("S-Light Distance", &playScene->spotLightDistance_, 1.0f, 100.0f);
			ImGui::SliderFloat("S-Light Decay", &playScene->spotLightDecay_, 0.1f, 10.0f);

			// 角度は度数法(Degree)で調整し、内部でラジアンに変換する
			static float angleDeg = 30.0f;
			if (ImGui::SliderFloat("Beam Angle", &angleDeg, 0.0f, 90.0f)) {
				playScene->spotLightAngle_ = angleDeg * (3.141592f / 180.0f);
			}
			static float falloffDeg = 10.0f;
			if (ImGui::SliderFloat("Falloff Start", &falloffDeg, 0.0f, 90.0f)) {
				playScene->spotLightFalloff_ = falloffDeg * (3.141592f / 180.0f);
			}
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