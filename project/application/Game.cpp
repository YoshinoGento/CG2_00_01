#include "Game.h"

#include "3d/Camera.h"
#include "base/DirectXCommon.h"
#include "base/FrameClock.h"
#include "base/ImGuiManager.h"
#include "base/Logger.h"
#include "base/SrvManager.h"
#include "editor/EditorShell.h"
#include "effect/PostEffectSystem.h"
#include "scene/GamePlayScene.h"
#include "scene/SceneFactory.h"
#include "scene/SceneManager.h"

#include <cassert>

Vector2 Game::mousePosInViewport_ = { 0.0f, 0.0f };

Game::Game() = default;
Game::~Game() = default;

void Game::Initialize() {
	Framework::Initialize();

	postEffectSystem_ = std::make_unique<PostEffectSystem>();
	if (!postEffectSystem_->Initialize(dxCommon_.get(), srvManager_.get())) {
		Logger::Log("Game::Initialize failed to initialize PostEffectSystem.");
		assert(false && "PostEffectSystem initialization failed");
	}

#ifdef USE_IMGUI
	editorShell_ = std::make_unique<EditorShell>();
	editorShell_->Initialize();
#endif

	sceneFactory_ = std::make_unique<SceneFactory>();
	SceneManager::GetInstance()->SetSceneFactory(sceneFactory_.get());
	SceneManager::GetInstance()->ChangeScene("TITLE");
}

void Game::Finalize() {
#ifdef USE_IMGUI
	if (editorShell_) {
		editorShell_->Finalize();
		editorShell_.reset();
	}
#endif

	SceneManager::DeleteInstance();
	if (postEffectSystem_) {
		postEffectSystem_->Finalize();
		postEffectSystem_.reset();
	}
	Framework::Finalize();
}

void Game::Update() {
	Framework::Update();
	SceneManager* sceneManager = SceneManager::GetInstance();
	sceneManager->BeginFrame();
	postEffectSystem_->Update(frameClock_->GetFrameDeltaSeconds());

	ImGuiManager::GetInstance()->Begin();
	sceneManager->PrepareFixedUpdate();
	while (frameClock_->ConsumeFixedStep()) {
		sceneManager->FixedUpdate(frameClock_->GetFixedDeltaSeconds());
	}

#ifdef USE_IMGUI
	if (editorShell_) {
		editorShell_->Draw(
			sceneManager->GetCurrentScene(),
			input_.get(),
			srvManager_.get(),
			postEffectSystem_.get());
		mousePosInViewport_ = editorShell_->GetMousePositionInViewport();
	}
#endif

	sceneManager->Update();
	ImGuiManager::GetInstance()->End();
}

void Game::Draw() {
	dxCommon_->PreDraw();
	srvManager_->PreDraw();

	SceneManager* sceneManager = SceneManager::GetInstance();
	// The scene is rendered into the offscreen RenderTexture first.
	sceneManager->Draw();

	float nearClip = 0.1f;
	float farClip = 1000.0f;
	if (BaseScene* currentScene = sceneManager->GetCurrentScene()) {
		if (GamePlayScene* playScene = dynamic_cast<GamePlayScene*>(currentScene)) {
			if (playScene->camera_) {
				nearClip = playScene->camera_->GetNearClip();
				farClip = playScene->camera_->GetFarClip();
			}
		}
	}
	postEffectSystem_->Execute(nearClip, farClip);

	// ImGui is composited on the swapchain after the gamma-corrected game image.
	ImGuiManager::GetInstance()->Draw();
	dxCommon_->RestoreRenderTextureToRenderTarget();
	dxCommon_->PostDraw();
}
