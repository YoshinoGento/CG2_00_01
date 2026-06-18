#include "Game.h"
#include "scene/SceneManager.h"
#include "scene/SceneFactory.h"
#include "scene/TitleScene.h"
#include "base/DirectXCommon.h"
#include "base/SrvManager.h"
#include "base/ImGuiManager.h"
#include "scene/GamePlayScene.h" 
#include "audio/Audio.h"
#include "3d/Object3d.h"
#include "2d/RuntimeTextTextureGenerator.h"
#include "2d/Sprite.h"
#include "2d/SpriteCommon.h"
#include "base/WinApp.h"
#include "debug/SkinningDebugWindow.h"
#include "FieldManager.h"
#include "FloatingTextSystem.h"
#include "GameplayEffectManager.h"
#include "effect/ParticleManager.h"
#include "effect/PostEffectManager.h"
#include "io/Input.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {
#if defined(_DEBUG) || defined(DEVELOPMENT_BUILD)
constexpr bool kAllowDebugUi = true;
#else
constexpr bool kAllowDebugUi = false;
#endif
constexpr float kVirtualScreenWidth = 1280.0f;
constexpr float kVirtualScreenHeight = 720.0f;
constexpr float kVirtualScreenAspect = kVirtualScreenWidth / kVirtualScreenHeight;
constexpr int kAutoDemoTileCount = 9;

void DrawHudSprite(Sprite* sprite, const Vector2& position, const Vector2& size, const Vector4& color) {
	if (!sprite) {
		return;
	}
	sprite->SetPosition(position);
	sprite->SetSize(size);
	sprite->SetColor(color);
	sprite->Update();
	sprite->Draw();
}

size_t GetFieldStateHudIndex(FieldState state) {
	switch (state) {
	case FieldState::Empty:
		return 0;
	case FieldState::Tilled:
		return 1;
	case FieldState::Watered:
		return 2;
	case FieldState::Planted:
		return 3;
	case FieldState::ReadyToHarvest:
		return 4;
	default:
		return 0;
	}
}

Vector4 GetFieldStateHudColor(FieldState state) {
	switch (state) {
	case FieldState::Empty:
		return { 0.45f, 0.75f, 0.28f, 1.0f };
	case FieldState::Tilled:
		return { 0.72f, 0.42f, 0.20f, 1.0f };
	case FieldState::Watered:
		return { 0.32f, 0.58f, 0.95f, 1.0f };
	case FieldState::Planted:
		return { 0.20f, 0.95f, 0.25f, 1.0f };
	case FieldState::ReadyToHarvest:
		return { 1.0f, 0.85f, 0.15f, 1.0f };
	default:
		return { 1.0f, 1.0f, 1.0f, 1.0f };
	}
}

Vector4 GetGrowthBarColor(float growth) {
	const float t = std::clamp(growth, 0.0f, 1.0f);
	return {
		0.30f + 0.70f * t,
		0.95f - 0.12f * t,
		0.25f,
		0.95f
	};
}

Vector4 GetMoistureBarColor(float moisture) {
	if (moisture < 0.2f) {
		return { 0.70f, 0.30f, 0.18f, 0.95f };
	}
	return { 0.25f, 0.62f + 0.22f * moisture, 1.0f, 0.95f };
}

int GetActionMessageIndex(FieldActionFeedbackType type) {
	switch (type) {
	case FieldActionFeedbackType::Tilled:
		return 0;
	case FieldActionFeedbackType::Watered:
		return 1;
	case FieldActionFeedbackType::Planted:
		return 2;
	case FieldActionFeedbackType::Harvested:
		return 3;
	case FieldActionFeedbackType::None:
	default:
		return -1;
	}
}
}

Vector2 Game::mousePosInViewport_ = { 0, 0 };

Game::Game() {
#if defined(_DEBUG) || defined(DEVELOPMENT_BUILD)
	presentationMode_ = false;
	hideDebugUI_ = false;
#else
	presentationMode_ = true;
	hideDebugUI_ = true;
#endif
	showGameplayHud_ = true;
}
Game::~Game() = default;

void Game::Initialize() {
	Framework::Initialize();
#if defined(_DEBUG) || defined(DEVELOPMENT_BUILD)
	presentationMode_ = false;
	hideDebugUI_ = false;
#else
	presentationMode_ = true;
	hideDebugUI_ = true;
#endif
	showGameplayHud_ = true;
	renderTextureSrvIndex_ = srvManager_->Allocate();
	srvManager_->CreateSRVforTexture2D(
		renderTextureSrvIndex_,
		dxCommon_->GetRenderTextureResource(),
		DXGI_FORMAT_R8G8B8A8_UNORM,
		1
	);
	postEffectResultSrvIndex_ = srvManager_->Allocate();
	srvManager_->CreateSRVforTexture2D(
		postEffectResultSrvIndex_,
		dxCommon_->GetPostEffectResultResource(),
		DXGI_FORMAT_R8G8B8A8_UNORM,
		1
	);
	finalDisplayTextureSrvIndex_ = srvManager_->Allocate();
	srvManager_->CreateSRVforTexture2D(
		finalDisplayTextureSrvIndex_,
		dxCommon_->GetFinalDisplayTextureResource(),
		DXGI_FORMAT_R8G8B8A8_UNORM,
		1
	);
	depthBufferSrvIndex_ = srvManager_->Allocate();
	srvManager_->CreateSRVforTexture2D(
		depthBufferSrvIndex_,
		dxCommon_->GetDepthBufferResource(),
		DXGI_FORMAT_R24_UNORM_X8_TYPELESS,
		1
	);
	normalTextureSrvIndex_ = srvManager_->Allocate();
	srvManager_->CreateSRVforTexture2D(
		normalTextureSrvIndex_,
		dxCommon_->GetNormalTextureResource(),
		DXGI_FORMAT_R8G8B8A8_UNORM,
		1
	);
	noiseNames_ = { "noise0.png", "noise1.png" };
	noiseSrvIndices_.clear();
	noiseSrvIndices_.reserve(noiseNames_.size());
	for (const std::string& noiseName : noiseNames_) {
		noiseSrvIndices_.push_back(spriteCommon_->LoadTexture("Resources/" + noiseName));
	}
	InitializeRuntimeTextTextures();
	InitializeGameplayHud();
	floatingTextSystem_ = std::make_unique<FloatingTextSystem>();
	floatingTextSystem_->Initialize(
		spriteCommon_.get(),
		"Resources/generated/text/harvest_gold_120.png");
	if (floatingTextSystem_->IsReady()) {
		floatingTextSystem_->RegisterTexture("action_tilled", "Resources/generated/text/action_tilled.png");
		floatingTextSystem_->RegisterTexture("action_watered", "Resources/generated/text/action_watered.png");
		floatingTextSystem_->RegisterTexture("action_planted", "Resources/generated/text/action_planted.png");
		floatingTextSystem_->RegisterTexture("action_harvested", "Resources/generated/text/action_harvested.png");
	}

	postProcess_ = std::make_unique<PostProcess>();
	postProcess_->Initialize(dxCommon_.get(), srvManager_.get());
	postEffectManager_ = std::make_unique<PostEffectManager>();
	postEffectManager_->Initialize(
		dxCommon_.get(),
		srvManager_.get(),
		renderTextureSrvIndex_,
		postEffectResultSrvIndex_,
		finalDisplayTextureSrvIndex_,
		depthBufferSrvIndex_,
		normalTextureSrvIndex_);
	gameplayEffectManager_ = std::make_unique<GameplayEffectManager>();
	gameplayEffectManager_->SetHarvestPopupDrawListEnabled(!floatingTextSystem_->IsReady());

	sceneFactory_ = std::make_unique<SceneFactory>();
	SceneManager::GetInstance()->SetSceneFactory(sceneFactory_.get());
	SceneManager::GetInstance()->ChangeScene("TITLE");
}

void Game::Finalize() {
	gameplayEffectManager_.reset();
	floatingTextSystem_.reset();
	postEffectManager_.reset();
	SceneManager::DeleteInstance();
	Framework::Finalize();
}

void Game::PlayHarvestEffect(const Vector3& position, int32_t price) {
	if (!gameplayEffectManager_) {
		return;
	}

	gameplayEffectManager_->PlayHarvestEffect(position, price);
	if (floatingTextSystem_) {
		const bool emphasizeAutoDemoFinalHarvest =
			autoDemoSequenceActive_ &&
			autoDemoStage_ == AutoDemoStage::HarvestTiles &&
			autoDemoTileCursor_ >= kAutoDemoTileCount;
		floatingTextSystem_->PlayRewardPopup(
			position + Vector3{ 0.0f, 1.2f, 0.0f },
			price,
			emphasizeAutoDemoFinalHarvest ? 1.38f : 1.18f,
			emphasizeAutoDemoFinalHarvest ? 1.45f : 1.25f);
	}

	GPUParticleEmitSettings particleSettings{};
	if (gameplayEffectManager_->ConsumeHarvestParticleEmitSettings(particleSettings) && particleManager_) {
		particleManager_->RequestGPUParticleEmit(particleSettings);
		particleManager_->EmitHarvestBurst(
			"Spark",
			position,
			gameplayEffectManager_->GetHarvestBurstParticleCount());
	}
	if (particleManager_) {
		particleManager_->PlayCropBurst(position, CropBurstLevel::Normal);
	}
}

void Game::PlayDigitalImpactEffect(const Vector3& position) {
	if (!gameplayEffectManager_) {
		return;
	}
	gameplayEffectManager_->PlayDigitalImpactEffect(position);

	GPUParticleEmitSettings particleSettings{};
	if (gameplayEffectManager_->ConsumeDigitalParticleEmitSettings(particleSettings) && particleManager_) {
		particleManager_->RequestGPUParticleEmit(particleSettings);
	}
}

void Game::UpdateGameplayEffects(float deltaTime) {
	if (!gameplayEffectManager_) {
		return;
	}
	gameplayEffectManager_->Update(deltaTime);
	if (floatingTextSystem_) {
		floatingTextSystem_->Update(deltaTime);
	}
}

void Game::DrawGameplayEffects() {
	if (!gameplayEffectManager_) {
		return;
	}

	Matrix4x4 viewProjectionMatrix = MatrixMath::MakeIdentity4x4();
	const Matrix4x4* viewProjection = nullptr;
	if (BaseScene* currentScene = SceneManager::GetInstance()->GetCurrentScene()) {
		if (GamePlayScene* playScene = dynamic_cast<GamePlayScene*>(currentScene)) {
			if (Camera* camera = playScene->GetCamera()) {
				viewProjectionMatrix = camera->GetViewProjectionMatrix();
				viewProjection = &viewProjectionMatrix;
			}
		}
	}
	gameplayEffectManager_->DrawGameplayEffects(
		gameViewportImageTopLeft_,
		gameViewportImageSize_,
		viewProjection);
}

void Game::DrawGameplayEffectImGui() {
	if (!gameplayEffectManager_) {
		return;
	}
	if (gameplayEffectManager_->DrawGameplayEffectImGui()) {
		PlayHarvestEffect(
			gameplayEffectManager_->GetDebugHarvestPosition(),
			gameplayEffectManager_->GetDebugHarvestPrice());
	}

	GPUParticleEmitSettings particleSettings{};
	if (gameplayEffectManager_->ConsumeDigitalParticleEmitSettings(particleSettings) && particleManager_) {
		particleManager_->RequestGPUParticleEmit(particleSettings);
	}
}

void Game::InitializeRuntimeTextTextures() {
	RuntimeTextTextureGenerator::GenerateFromJson("Resources/text_textures.json");
}

void Game::InitializeGameplayHud() {
	if (!spriteCommon_) {
		return;
	}

	hudWhiteTextureHandle_ = spriteCommon_->LoadTexture("Resources/human/white.png");
	hudControlsLine1TextureHandle_ = spriteCommon_->LoadTexture("Resources/generated/text/field_controls_line1.png");
	hudControlsLine2TextureHandle_ = spriteCommon_->LoadTexture("Resources/generated/text/field_controls_line2.png");
	hudSelectedLabelTextureHandle_ = spriteCommon_->LoadTexture("Resources/generated/text/field_selected_label.png");
	hudGrowthLabelTextureHandle_ = spriteCommon_->LoadTexture("Resources/generated/text/field_growth_label.png");
	hudMoistureLabelTextureHandle_ = spriteCommon_->LoadTexture("Resources/generated/text/field_moisture_label.png");
	hudStateTextureHandles_[0] = spriteCommon_->LoadTexture("Resources/generated/text/field_state_empty.png");
	hudStateTextureHandles_[1] = spriteCommon_->LoadTexture("Resources/generated/text/field_state_tilled.png");
	hudStateTextureHandles_[2] = spriteCommon_->LoadTexture("Resources/generated/text/field_state_watered.png");
	hudStateTextureHandles_[3] = spriteCommon_->LoadTexture("Resources/generated/text/field_state_planted.png");
	hudStateTextureHandles_[4] = spriteCommon_->LoadTexture("Resources/generated/text/field_state_ready.png");
	hudNextTextureHandles_[0] = spriteCommon_->LoadTexture("Resources/generated/text/field_next_empty.png");
	hudNextTextureHandles_[1] = spriteCommon_->LoadTexture("Resources/generated/text/field_next_tilled.png");
	hudNextTextureHandles_[2] = spriteCommon_->LoadTexture("Resources/generated/text/field_next_watered.png");
	hudNextTextureHandles_[3] = spriteCommon_->LoadTexture("Resources/generated/text/field_next_planted.png");
	hudNextTextureHandles_[4] = spriteCommon_->LoadTexture("Resources/generated/text/field_next_ready.png");

	for (int percent = 0; percent <= 100; ++percent) {
		char outputPath[128]{};
		char text[16]{};
		std::snprintf(outputPath, sizeof(outputPath), "Resources/generated/text/field_percent_%03d.png", percent);
		std::snprintf(text, sizeof(text), "%d%%", percent);

		RuntimeTextTextureGenerator::TextTextureRequest request{};
		request.id = "field_percent";
		request.text = text;
		request.font = "Resources/fonts/KiwiMaru-Medium.ttf";
		request.output = outputPath;
		request.fontSize = 24.0f;
		request.color = { 1.0f, 1.0f, 0.92f, 1.0f };
		request.shadowColor = { 0.0f, 0.0f, 0.0f, 0.85f };
		request.shadowOffset = { 2.0f, 2.0f };
		request.padding = { 10.0f, 7.0f };
		request.overwriteIfExists = false;
		RuntimeTextTextureGenerator::GenerateTextTexture(request);
		hudPercentTextureHandles_[percent] = spriteCommon_->LoadTexture(outputPath);
	}

	auto makeSprite = [&](uint32_t textureHandle) {
		auto sprite = std::make_unique<Sprite>();
		sprite->Initialize(spriteCommon_.get(), textureHandle);
		sprite->SetAnchorPoint({ 0.0f, 0.0f });
		return sprite;
	};

	hudControlsPanelSprite_ = makeSprite(hudWhiteTextureHandle_);
	hudControlsLine1Sprite_ = makeSprite(hudControlsLine1TextureHandle_);
	hudControlsLine2Sprite_ = makeSprite(hudControlsLine2TextureHandle_);
	hudStatusPanelSprite_ = makeSprite(hudWhiteTextureHandle_);
	hudSelectedLabelSprite_ = makeSprite(hudSelectedLabelTextureHandle_);
	hudStateValueSprite_ = makeSprite(hudStateTextureHandles_[0]);
	hudGrowthLabelSprite_ = makeSprite(hudGrowthLabelTextureHandle_);
	hudMoistureLabelSprite_ = makeSprite(hudMoistureLabelTextureHandle_);
	hudGrowthBarBackgroundSprite_ = makeSprite(hudWhiteTextureHandle_);
	hudGrowthBarFillSprite_ = makeSprite(hudWhiteTextureHandle_);
	hudMoistureBarBackgroundSprite_ = makeSprite(hudWhiteTextureHandle_);
	hudMoistureBarFillSprite_ = makeSprite(hudWhiteTextureHandle_);
	hudStateAccentSprite_ = makeSprite(hudWhiteTextureHandle_);
	hudNextActionSprite_ = makeSprite(hudNextTextureHandles_[0]);
	hudGrowthPercentSprite_ = makeSprite(hudPercentTextureHandles_[0]);
	hudMoisturePercentSprite_ = makeSprite(hudPercentTextureHandles_[0]);
}

void Game::DrawGameplayEffectSprites() {
	if (!floatingTextSystem_ || !floatingTextSystem_->IsReady() || !spriteCommon_) {
		return;
	}

	Matrix4x4 viewProjectionMatrix = MatrixMath::MakeIdentity4x4();
	const Matrix4x4* viewProjection = nullptr;
	if (BaseScene* currentScene = SceneManager::GetInstance()->GetCurrentScene()) {
		if (GamePlayScene* playScene = dynamic_cast<GamePlayScene*>(currentScene)) {
			if (Camera* camera = playScene->GetCamera()) {
				viewProjectionMatrix = camera->GetViewProjectionMatrix();
				viewProjection = &viewProjectionMatrix;
			}
		}
	}
	if (!viewProjection) {
		return;
	}

	// Sprites are rendered into SceneRenderTexture, so the projection target is
	// the fixed virtual render size. The Game Viewport then scales this texture.
	floatingTextSystem_->Draw(
		{ 0.0f, 0.0f },
		{ kVirtualScreenWidth, kVirtualScreenHeight },
		viewProjection);
}

void Game::DrawGameplayHud(GamePlayScene* playScene) {
	if (!showGameplayHud_ || !playScene || !playScene->GetFieldManager() || !spriteCommon_) {
		return;
	}

	const FieldTile* selectedTile = playScene->GetFieldManager()->GetSelectedTile();
	if (!selectedTile) {
		return;
	}

	spriteCommon_->PreDraw();

	DrawHudSprite(
		hudControlsPanelSprite_.get(),
		{ 16.0f, 548.0f },
		{ 1110.0f, 148.0f },
		{ 0.02f, 0.035f, 0.03f, 0.62f });
	DrawHudSprite(
		hudControlsLine1Sprite_.get(),
		{ 28.0f, 566.0f },
		{ 760.0f, 38.0f },
		{ 1.0f, 1.0f, 1.0f, 1.0f });
	DrawHudSprite(
		hudControlsLine2Sprite_.get(),
		{ 28.0f, 612.0f },
		{ 1060.0f, 38.0f },
		{ 1.0f, 1.0f, 1.0f, 1.0f });

	DrawHudSprite(
		hudStatusPanelSprite_.get(),
		{ 910.0f, 24.0f },
		{ 342.0f, 176.0f },
		{ 0.02f, 0.035f, 0.03f, 0.66f });
	DrawHudSprite(
		hudSelectedLabelSprite_.get(),
		{ 928.0f, 38.0f },
		{ 230.0f, 42.0f },
		{ 1.0f, 1.0f, 1.0f, 1.0f });

	const size_t stateIndex = GetFieldStateHudIndex(selectedTile->state);
	const Vector4 stateColor = GetFieldStateHudColor(selectedTile->state);
	DrawHudSprite(
		hudStateAccentSprite_.get(),
		{ 910.0f, 24.0f },
		{ 8.0f, 176.0f },
		{ stateColor.x, stateColor.y, stateColor.z, 0.95f });

	if (stateIndex < hudStateTextureHandles_.size() && hudStateValueSprite_) {
		hudStateValueSprite_->SetTexture(hudStateTextureHandles_[stateIndex]);
	}
	DrawHudSprite(
		hudStateValueSprite_.get(),
		{ 928.0f, 82.0f },
		{ 230.0f, 36.0f },
		stateColor);

	if (stateIndex < hudNextTextureHandles_.size() && hudNextActionSprite_) {
		hudNextActionSprite_->SetTexture(hudNextTextureHandles_[stateIndex]);
	}
	DrawHudSprite(
		hudNextActionSprite_.get(),
		{ 928.0f, 116.0f },
		{ 292.0f, 32.0f },
		{ 1.0f, 1.0f, 1.0f, 1.0f });

	const float growth = std::clamp(selectedTile->growth, 0.0f, 1.0f);
	const float moisture = std::clamp(selectedTile->moisture, 0.0f, 1.0f);
	const int growthPercent = std::clamp(static_cast<int>(std::round(growth * 100.0f)), 0, 100);
	const int moisturePercent = std::clamp(static_cast<int>(std::round(moisture * 100.0f)), 0, 100);
	const Vector2 barPosition = { 1012.0f, 154.0f };
	const Vector2 barSize = { 166.0f, 14.0f };

	DrawHudSprite(
		hudGrowthLabelSprite_.get(),
		{ 928.0f, 144.0f },
		{ 80.0f, 32.0f },
		{ 1.0f, 1.0f, 1.0f, 1.0f });
	if (hudGrowthPercentSprite_) {
		hudGrowthPercentSprite_->SetTexture(hudPercentTextureHandles_[growthPercent]);
	}
	DrawHudSprite(
		hudGrowthPercentSprite_.get(),
		{ 1184.0f, 143.0f },
		{ 48.0f, 30.0f },
		{ 1.0f, 1.0f, 1.0f, 1.0f });
	DrawHudSprite(
		hudGrowthBarBackgroundSprite_.get(),
		barPosition,
		barSize,
		{ 0.12f, 0.16f, 0.12f, 0.88f });
	DrawHudSprite(
		hudGrowthBarFillSprite_.get(),
		barPosition,
		{ barSize.x * growth, barSize.y },
		GetGrowthBarColor(growth));

	DrawHudSprite(
		hudMoistureLabelSprite_.get(),
		{ 928.0f, 176.0f },
		{ 90.0f, 32.0f },
		{ 1.0f, 1.0f, 1.0f, 1.0f });
	if (hudMoisturePercentSprite_) {
		hudMoisturePercentSprite_->SetTexture(hudPercentTextureHandles_[moisturePercent]);
	}
	DrawHudSprite(
		hudMoisturePercentSprite_.get(),
		{ 1184.0f, 175.0f },
		{ 48.0f, 30.0f },
		{ 1.0f, 1.0f, 1.0f, 1.0f });
	DrawHudSprite(
		hudMoistureBarBackgroundSprite_.get(),
		{ barPosition.x, 186.0f },
		barSize,
		{ 0.10f, 0.13f, 0.16f, 0.88f });
	DrawHudSprite(
		hudMoistureBarFillSprite_.get(),
		{ barPosition.x, 186.0f },
		{ barSize.x * moisture, barSize.y },
		GetMoistureBarColor(moisture));
}

void Game::ShowFieldActionMessage(FieldActionFeedbackType type, const Vector3& worldPosition) {
	const int index = GetActionMessageIndex(type);
	if (index < 0) {
		return;
	}
	if (!floatingTextSystem_ || !floatingTextSystem_->IsReady()) {
		return;
	}

	static constexpr const char* kActionTextureIds[] = {
		"action_tilled",
		"action_watered",
		"action_planted",
		"action_harvested",
	};
	static constexpr Vector4 kActionColors[] = {
		{ 1.0f, 0.78f, 0.42f, 1.0f },
		{ 0.58f, 0.90f, 1.0f, 1.0f },
		{ 0.55f, 1.0f, 0.50f, 1.0f },
		{ 1.0f, 0.90f, 0.35f, 1.0f },
	};

	const bool emphasizeAutoDemoFinalHarvest =
		autoDemoSequenceActive_ &&
		autoDemoStage_ == AutoDemoStage::HarvestTiles &&
		autoDemoTileCursor_ >= kAutoDemoTileCount &&
		type == FieldActionFeedbackType::Harvested;

	floatingTextSystem_->PlayFloatingText(
		worldPosition,
		kActionTextureIds[index],
		emphasizeAutoDemoFinalHarvest ? Vector2{ 232.0f, 88.0f } : Vector2{ 190.0f, 72.0f },
		kActionColors[index],
		emphasizeAutoDemoFinalHarvest ? 1.15f : 0.85f,
		emphasizeAutoDemoFinalHarvest ? Vector3{ 0.0f, 0.82f, 0.0f } : Vector3{ 0.0f, 0.65f, 0.0f });
}

void Game::ApplyDemoRecordingModeSettings() {
	demoRecordingMode_ = true;
	SetPresentationMode(true);
	showGameplayHud_ = true;
	if (gameplayEffectManager_) {
		gameplayEffectManager_->ApplyRecordingDemoDefaults();
	}
}

void Game::SetPresentationMode(bool enabled) {
	presentationMode_ = enabled;
	if (presentationMode_) {
		hideDebugUI_ = true;
	} else {
#if defined(_DEBUG) || defined(DEVELOPMENT_BUILD)
		hideDebugUI_ = false;
#else
		hideDebugUI_ = true;
#endif
	}
}

void Game::StartAutoDemoSequence() {
	ApplyDemoRecordingModeSettings();
	autoDemoSequenceActive_ = true;
	autoDemoStage_ = AutoDemoStage::WaitBeforeTill;
	autoDemoTimer_ = 0.0f;
	autoDemoTileCursor_ = 0;

	BaseScene* current = SceneManager::GetInstance()->GetCurrentScene();
	GamePlayScene* playScene = dynamic_cast<GamePlayScene*>(current);
	if (playScene) {
		playScene->ApplyAutoDemoScenePreset();
		if (FieldManager* fieldManager = playScene->GetFieldManager()) {
			fieldManager->ResetAllTilesForAutoDemo(0);
			fieldManager->SelectTile(0);
		}
	}
}

void Game::StopAutoDemoSequence() {
	autoDemoSequenceActive_ = false;
	autoDemoStage_ = AutoDemoStage::Idle;
	autoDemoTimer_ = 0.0f;
	autoDemoTileCursor_ = 0;

	BaseScene* current = SceneManager::GetInstance()->GetCurrentScene();
	GamePlayScene* playScene = dynamic_cast<GamePlayScene*>(current);
	if (playScene) {
		playScene->SetFieldSelectionEnabled(true);
		playScene->SetSkyboxInputEnabled(true);
		playScene->SetFieldInputEnabled(true);
		playScene->SetCameraInputEnabled(true);
	}
}

void Game::UpdateAutoDemoSequence(float deltaTime) {
	if (!autoDemoSequenceActive_ || !gameplayEffectManager_) {
		return;
	}

	autoDemoTimer_ += std::clamp(deltaTime, 0.0f, 0.25f);
	BaseScene* current = SceneManager::GetInstance()->GetCurrentScene();
	GamePlayScene* playScene = dynamic_cast<GamePlayScene*>(current);
	FieldManager* fieldManager = playScene ? playScene->GetFieldManager() : nullptr;
	constexpr int kFirstTileIndex = 0;
	constexpr int kCenterTileIndex = 4;
	constexpr float kInitialPauseDuration = 0.80f;
	constexpr float kWorkTileInterval = 0.45f;
	constexpr float kPhasePauseDuration = 0.70f;
	constexpr float kGrowthDuration = 2.00f;
	constexpr float kReadyViewDuration = 1.00f;
	constexpr float kHarvestTileInterval = 0.65f;
	constexpr float kFinalHarvestTileInterval = 0.80f;
	constexpr float kDigitalImpactDelay = 0.60f;
	constexpr float kDemoFinishViewDuration = 1.50f;

	if (!playScene || !fieldManager) {
		StopAutoDemoSequence();
		return;
	}

	playScene->SetFieldSelectionEnabled(false);
	playScene->SetSkyboxInputEnabled(false);
	playScene->SetFieldInputEnabled(false);
	playScene->SetCameraInputEnabled(false);
	playScene->SetDemoCameraPreset();

	auto runTileAction = [&](float interval, auto&& action, AutoDemoStage nextStage) {
		if (autoDemoTimer_ < interval) {
			if (autoDemoTileCursor_ < fieldManager->GetTileCount()) {
				fieldManager->SelectTile(autoDemoTileCursor_);
			}
			return;
		}

		if (autoDemoTileCursor_ < fieldManager->GetTileCount()) {
			fieldManager->SelectTile(autoDemoTileCursor_);
			action(autoDemoTileCursor_);
			++autoDemoTileCursor_;
			autoDemoTimer_ = 0.0f;
			return;
		}

		autoDemoTileCursor_ = 0;
		autoDemoTimer_ = 0.0f;
		autoDemoStage_ = nextStage;
	};

	switch (autoDemoStage_) {
	case AutoDemoStage::InitializeField:
		playScene->ApplyAutoDemoScenePreset();
		fieldManager->ResetAllTilesForAutoDemo(kFirstTileIndex);
		autoDemoTileCursor_ = 0;
		autoDemoStage_ = AutoDemoStage::WaitBeforeTill;
		autoDemoTimer_ = 0.0f;
		break;
	case AutoDemoStage::WaitBeforeTill:
		fieldManager->SelectTile(kFirstTileIndex);
		if (autoDemoTimer_ >= kInitialPauseDuration) {
			autoDemoStage_ = AutoDemoStage::TillTiles;
			autoDemoTimer_ = 0.0f;
		}
		break;
	case AutoDemoStage::TillTiles:
		runTileAction(
			kWorkTileInterval,
			[&](int index) { fieldManager->TillTile(index); },
			AutoDemoStage::WaitAfterTill);
		break;
	case AutoDemoStage::WaitAfterTill:
		if (autoDemoTimer_ >= kPhasePauseDuration) {
			autoDemoStage_ = AutoDemoStage::WaterTiles;
			autoDemoTileCursor_ = 0;
			autoDemoTimer_ = 0.0f;
		}
		break;
	case AutoDemoStage::WaterTiles:
		runTileAction(
			kWorkTileInterval,
			[&](int index) { fieldManager->WaterTile(index); },
			AutoDemoStage::WaitAfterWater);
		break;
	case AutoDemoStage::WaitAfterWater:
		if (autoDemoTimer_ >= kPhasePauseDuration) {
			autoDemoStage_ = AutoDemoStage::PlantTiles;
			autoDemoTileCursor_ = 0;
			autoDemoTimer_ = 0.0f;
		}
		break;
	case AutoDemoStage::PlantTiles:
		runTileAction(
			kWorkTileInterval,
			[&](int index) { fieldManager->PlantTile(index); },
			AutoDemoStage::GrowTiles);
		break;
	case AutoDemoStage::GrowTiles:
		fieldManager->SelectTile(kCenterTileIndex);
		if (fieldManager->FastForwardAllGrowth(deltaTime / kGrowthDuration) || autoDemoTimer_ >= kGrowthDuration) {
			autoDemoStage_ = AutoDemoStage::WaitReady;
			autoDemoTimer_ = 0.0f;
		}
		break;
	case AutoDemoStage::WaitReady:
		fieldManager->SelectTile(kCenterTileIndex);
		if (autoDemoTimer_ >= kReadyViewDuration) {
			autoDemoStage_ = AutoDemoStage::HarvestTiles;
			autoDemoTileCursor_ = 0;
			autoDemoTimer_ = 0.0f;
		}
		break;
	case AutoDemoStage::HarvestTiles:
		runTileAction(
			autoDemoTileCursor_ == fieldManager->GetTileCount() - 1 ? kFinalHarvestTileInterval : kHarvestTileInterval,
			[&](int index) { fieldManager->HarvestTile(index); },
			AutoDemoStage::WaitBeforeDigitalImpact);
		break;
	case AutoDemoStage::WaitBeforeDigitalImpact:
		fieldManager->SelectTile(kCenterTileIndex);
		if (autoDemoTimer_ >= kDigitalImpactDelay) {
			autoDemoStage_ = AutoDemoStage::DigitalImpact;
			autoDemoTimer_ = 0.0f;
		}
		break;
	case AutoDemoStage::DigitalImpact:
		fieldManager->SelectTile(kCenterTileIndex);
		if (const FieldTile* selectedTile = fieldManager->GetSelectedTile()) {
			PlayDigitalImpactEffect(selectedTile->worldPosition + Vector3{ 0.0f, 0.7f, 0.0f });
		}
		autoDemoStage_ = AutoDemoStage::Finished;
		autoDemoTimer_ = 0.0f;
		break;
	case AutoDemoStage::Finished:
		if (autoDemoTimer_ >= kDemoFinishViewDuration) {
			StopAutoDemoSequence();
		}
		break;
	case AutoDemoStage::Idle:
	default:
		autoDemoSequenceActive_ = false;
		break;
	}
}

void Game::DrawDemoRecordingImGui(GamePlayScene* playScene) {
	ImGui::Begin("Demo Recording");

	if (ImGui::Checkbox("Presentation Mode", &presentationMode_)) {
		SetPresentationMode(presentationMode_);
	}
	if (ImGui::Checkbox("Demo Recording Mode", &demoRecordingMode_)) {
		if (demoRecordingMode_) {
			ApplyDemoRecordingModeSettings();
		} else if (gameplayEffectManager_) {
			gameplayEffectManager_->SetDemoMode(false);
		}
	}
	ImGui::Checkbox("Hide Debug UI", &hideDebugUI_);

	if (ImGui::Button(autoDemoSequenceActive_ ? "Stop Auto Farm Demo" : "Auto Farm Demo")) {
		if (autoDemoSequenceActive_) {
			StopAutoDemoSequence();
		} else {
			StartAutoDemoSequence();
		}
	}

	if (gameplayEffectManager_) {
		if (ImGui::Button("Play Normal Harvest")) {
			PlayHarvestEffect(
				gameplayEffectManager_->GetDebugHarvestPosition(),
				gameplayEffectManager_->GetDebugHarvestPrice());
		}
		ImGui::SameLine();
		if (ImGui::Button("Play Digital Rare Harvest")) {
			PlayHarvestEffect(
				gameplayEffectManager_->GetDebugDigitalImpactPosition(),
				gameplayEffectManager_->GetDebugHarvestPrice());
			PlayDigitalImpactEffect(gameplayEffectManager_->GetDebugDigitalImpactPosition());
		}
	}

	ImGui::Separator();
	ImGui::Text("Auto Farm Demo: %s", autoDemoSequenceActive_ ? "Playing" : "Stopped");
	ImGui::Text("Auto Timer: %.2f", autoDemoTimer_);
	ImGui::Text("Borderless Fullscreen: %s", (winApp_ && winApp_->IsBorderlessFullscreen()) ? "ON" : "OFF");
	ImGui::TextUnformatted("Keys: I=Harvest, J=Digital, F2=HUD, F5=Auto Farm Demo, F7=Presentation, F1=Debug UI, F11=Borderless");
	ImGui::Text("Gameplay HUD: %s", showGameplayHud_ ? "ON" : "OFF");
	ImGui::Text("Presentation Mode: %s", presentationMode_ ? "ON" : "OFF");
	if (playScene && playScene->GetSkyboxManager()) {
		ImGui::Text("Current Skybox: %s", playScene->GetSkyboxManager()->GetCurrentSkyboxName().c_str());
	}
	ImGui::TextUnformatted("Game Viewport Source: FinalDisplayTexture");
	ImGui::TextUnformatted("GammaCorrection: LinearToSRGB once at final display");

	ImGui::End();
}

void Game::Update() {
	Framework::Update();
	const auto now = std::chrono::steady_clock::now();
	float randomNoiseDeltaTime = 1.0f / 60.0f;
	if (previousRandomNoiseTime_.time_since_epoch().count() != 0) {
		randomNoiseDeltaTime = std::chrono::duration<float>(now - previousRandomNoiseTime_).count();
		randomNoiseDeltaTime = std::clamp(randomNoiseDeltaTime, 1.0f / 240.0f, 1.0f / 15.0f);
	}
	previousRandomNoiseTime_ = now;
	UpdateGameplayEffects(randomNoiseDeltaTime);
	const GameplayEffectManager::ScreenPostEffectModifier gameplayPostEffectModifier =
		gameplayEffectManager_ ? gameplayEffectManager_->GetScreenPostEffectModifier() : GameplayEffectManager::ScreenPostEffectModifier{};
	if (fullscreenRandomNoiseAnimate_ || gameplayPostEffectModifier.randomNoiseAnimate) {
		fullscreenRandomNoiseTime_ += randomNoiseDeltaTime * std::clamp(fullscreenRandomNoiseTimeSpeed_, 0.0f, 10.0f);
	}

	BaseScene* current = SceneManager::GetInstance()->GetCurrentScene();
	GamePlayScene* playScene = dynamic_cast<GamePlayScene*>(current);
	const bool isTitleScene = dynamic_cast<TitleScene*>(current) != nullptr;

	ImGuiManager::GetInstance()->Begin();
	if (playScene && !hideDebugUI_ && !presentationMode_) {
		ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
	}
	if (input_ && !ImGui::GetIO().WantCaptureKeyboard) {
#if defined(_DEBUG) || defined(DEVELOPMENT_BUILD)
		if (input_->TriggerKey(DIK_F1)) {
			hideDebugUI_ = !hideDebugUI_;
		}
#endif
		if (input_->TriggerKey(DIK_F2)) {
			showGameplayHud_ = !showGameplayHud_;
		}
		if (input_->TriggerKey(DIK_F7)) {
			SetPresentationMode(!presentationMode_);
		}
		if (playScene && input_->TriggerKey(DIK_F5)) {
			if (autoDemoSequenceActive_) {
				StopAutoDemoSequence();
			} else {
				StartAutoDemoSequence();
			}
		}
		if (input_->TriggerKey(DIK_F11) && winApp_) {
			winApp_->ToggleBorderlessFullscreen();
		}
		if (!autoDemoSequenceActive_ && playScene && input_->TriggerKey(DIK_J) && gameplayEffectManager_) {
			PlayDigitalImpactEffect(gameplayEffectManager_->GetDebugDigitalImpactPosition());
		}
	}
	UpdateAutoDemoSequence(randomNoiseDeltaTime);

	if (playScene) {
		playScene->viewportHovered_ = false;
		playScene->viewportImageSize_ = { 0.0f, 0.0f };
	}
	gameViewportImageTopLeft_ = { 0.0f, 0.0f };
	gameViewportImageSize_ = { 0.0f, 0.0f };

	// Game Viewport displays the gamma-corrected final display texture.
	ImGuiWindowFlags gameViewportWindowFlags = 0;
	const bool presentationViewport = hideDebugUI_ || presentationMode_ || isTitleScene;
	if (presentationViewport) {
		ImGuiViewport* mainViewport = ImGui::GetMainViewport();
		const ImVec2 availableSize = mainViewport->WorkSize;
		const float viewportScale = std::clamp(
			(std::min)(
				availableSize.x / kVirtualScreenWidth,
				availableSize.y / kVirtualScreenHeight),
			0.1f,
			1.0f);
		const ImVec2 viewportSize = {
			kVirtualScreenWidth * viewportScale,
			kVirtualScreenHeight * viewportScale,
		};
		const ImVec2 viewportPos = {
			mainViewport->WorkPos.x + (availableSize.x - viewportSize.x) * 0.5f,
			mainViewport->WorkPos.y + (availableSize.y - viewportSize.y) * 0.5f,
		};
		ImGui::SetNextWindowPos(viewportPos, ImGuiCond_Always);
		ImGui::SetNextWindowSize(viewportSize, ImGuiCond_Always);
		gameViewportWindowFlags =
			ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoResize |
			ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoScrollbar |
			ImGuiWindowFlags_NoScrollWithMouse |
			ImGuiWindowFlags_NoBringToFrontOnFocus |
			ImGuiWindowFlags_NoDocking;
	}
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	if (ImGui::Begin("Game Viewport", nullptr, gameViewportWindowFlags)) {
		ImVec2 contentSize = ImGui::GetContentRegionAvail();
		if (contentSize.x > 1.0f && contentSize.y > 1.0f) {
			float targetAspect = kVirtualScreenAspect;
			ImVec2 displaySize = contentSize;
			if (displaySize.x / displaySize.y > targetAspect) displaySize.x = displaySize.y * targetAspect;
			else displaySize.y = displaySize.x / targetAspect;

			if (displaySize.x > 1.0f && displaySize.y > 1.0f) {
				ImVec2 offset = { (contentSize.x - displaySize.x) * 0.5f, (contentSize.y - displaySize.y) * 0.5f };
				const Vector2 shakeOffset = gameplayEffectManager_
					? gameplayEffectManager_->GetViewportShakeOffset()
					: Vector2{ 0.0f, 0.0f };
				ImGui::SetCursorPos({
					ImGui::GetCursorPos().x + offset.x + shakeOffset.x,
					ImGui::GetCursorPos().y + offset.y + shakeOffset.y,
					});

				ImVec2 mousePos = ImGui::GetIO().MousePos;
				ImVec2 imageTopLeft = ImGui::GetCursorScreenPos();
				const bool mouseInsideImage =
					mousePos.x >= imageTopLeft.x &&
					mousePos.y >= imageTopLeft.y &&
					mousePos.x < imageTopLeft.x + displaySize.x &&
					mousePos.y < imageTopLeft.y + displaySize.y;
				if (mouseInsideImage) {
					mousePosInViewport_.x = (mousePos.x - imageTopLeft.x) / displaySize.x * kVirtualScreenWidth;
					mousePosInViewport_.y = (mousePos.y - imageTopLeft.y) / displaySize.y * kVirtualScreenHeight;
				} else {
					mousePosInViewport_ = { -1.0f, -1.0f };
				}

				ImGui::Image((ImTextureID)srvManager_->GetGPUDescriptorHandle(finalDisplayTextureSrvIndex_).ptr, displaySize);
				const ImVec2 imageMin = ImGui::GetItemRectMin();
				const ImVec2 imageMax = ImGui::GetItemRectMax();
				const ImVec2 imageSize = {
					imageMax.x - imageMin.x,
					imageMax.y - imageMin.y,
				};
				gameViewportImageTopLeft_ = { imageMin.x, imageMin.y };
				gameViewportImageSize_ = { imageSize.x, imageSize.y };
				if (playScene) {
					playScene->SetViewportInfo(
						{ imageMin.x, imageMin.y },
						{ imageSize.x, imageSize.y },
						{ mousePos.x, mousePos.y },
						ImGui::IsItemHovered() && mouseInsideImage);
				}
				DrawGameplayEffects();
			}
		}
	}
	ImGui::End();
	ImGui::PopStyleVar();

	const bool showDebugWindows = playScene && !hideDebugUI_ && !presentationMode_;
	if (showDebugWindows) {
		DrawDemoRecordingImGui(playScene);
	}

	if (showDebugWindows && playScene) {
		ImGui::Begin("Global Settings");
		static const char* targets[] = { "None", "Sprite", "Object3D", "Particle", "Sphere" };
		ImGui::Combo("Edit Focus", &playScene->selectedTarget_, targets, 5);
		ImGui::Separator();
		if (SkyboxManager* skyboxManager = playScene->GetSkyboxManager()) {
			skyboxManager->DrawImGui();
		}
		if (FieldManager* fieldManager = playScene->GetFieldManager()) {
			fieldManager->DrawImGui();
		}
		if (ImGui::CollapsingHeader("Mouse Picking Debug", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Text("Mouse Screen Pos: %.1f, %.1f",
				playScene->viewportMousePosition_.x,
				playScene->viewportMousePosition_.y);
			ImGui::Text("Mouse Virtual Pos: %.1f, %.1f",
				playScene->fieldMouseVirtualPosition_.x,
				playScene->fieldMouseVirtualPosition_.y);
			ImGui::Text("Mouse In Game Viewport: %s", playScene->fieldMouseInViewport_ ? "true" : "false");
			ImGui::Text("Ray Valid: %s", playScene->fieldMouseRayValid_ ? "true" : "false");
			ImGui::Text("Ray Origin: %.2f, %.2f, %.2f",
				playScene->fieldMouseRayOrigin_.x,
				playScene->fieldMouseRayOrigin_.y,
				playScene->fieldMouseRayOrigin_.z);
			ImGui::Text("Ray Direction: %.2f, %.2f, %.2f",
				playScene->fieldMouseRayDirection_.x,
				playScene->fieldMouseRayDirection_.y,
				playScene->fieldMouseRayDirection_.z);
			ImGui::Text("Hit Valid: %s", playScene->fieldMouseHit_ ? "true" : "false");
			ImGui::Text("Hit Position: %.2f, %.2f, %.2f",
				playScene->fieldMouseHitPosition_.x,
				playScene->fieldMouseHitPosition_.y,
				playScene->fieldMouseHitPosition_.z);
			ImGui::Text("Selected Tile Index: %d", playScene->fieldMouseSelectedIndex_);
			ImGui::Text("Fullscreen: %s", (winApp_ && winApp_->IsBorderlessFullscreen()) ? "true" : "false");
			ImGui::Text("Game Viewport Rect: %.1f, %.1f, %.1f, %.1f",
				playScene->viewportImageTopLeft_.x,
				playScene->viewportImageTopLeft_.y,
				playScene->viewportImageSize_.x,
				playScene->viewportImageSize_.y);
		}
		if (ImGui::CollapsingHeader("Auto Demo / Field Debug", ImGuiTreeNodeFlags_DefaultOpen)) {
			auto autoDemoStageName = [](AutoDemoStage stage) -> const char* {
				switch (stage) {
				case AutoDemoStage::Idle: return "Idle";
				case AutoDemoStage::InitializeField: return "InitializeField";
				case AutoDemoStage::WaitBeforeTill: return "WaitBeforeTill";
				case AutoDemoStage::TillTiles: return "TillTiles";
				case AutoDemoStage::WaitAfterTill: return "WaitAfterTill";
				case AutoDemoStage::WaterTiles: return "WaterTiles";
				case AutoDemoStage::WaitAfterWater: return "WaitAfterWater";
				case AutoDemoStage::PlantTiles: return "PlantTiles";
				case AutoDemoStage::GrowTiles: return "GrowTiles";
				case AutoDemoStage::WaitReady: return "WaitReady";
				case AutoDemoStage::HarvestTiles: return "HarvestTiles";
				case AutoDemoStage::WaitBeforeDigitalImpact: return "WaitBeforeDigitalImpact";
				case AutoDemoStage::DigitalImpact: return "DigitalImpact";
				case AutoDemoStage::Finished: return "Finished";
				}
				return "Unknown";
				};

			FieldManager* fieldManager = playScene->GetFieldManager();
			const Vector3 fieldCenter = fieldManager ? fieldManager->GetFieldCenter() : Vector3{};
			const int visibleTileCount = fieldManager ? fieldManager->GetVisibleTileCount() : 0;
			ImGui::Text("Auto Demo Active: %s", autoDemoSequenceActive_ ? "true" : "false");
			ImGui::Text("Auto Demo Phase: %s", autoDemoStageName(autoDemoStage_));
			ImGui::Text("Camera Position: %.2f, %.2f, %.2f", playScene->cameraPos_.x, playScene->cameraPos_.y, playScene->cameraPos_.z);
			ImGui::Text("Camera Rotation: %.2f, %.2f, %.2f", playScene->cameraRot_.x, playScene->cameraRot_.y, playScene->cameraRot_.z);
			ImGui::Text("Field Center: %.2f, %.2f, %.2f", fieldCenter.x, fieldCenter.y, fieldCenter.z);
			ImGui::Text("Selected Tile Index: %d", fieldManager ? fieldManager->GetSelectedIndex() : -1);
			ImGui::Text("Visible Tile Count: %d", visibleTileCount);
			ImGui::Text("Grid Visible: %s", playScene->showDebugGrid_ ? "true" : "false");
			ImGui::Text("Field Visible: %s", visibleTileCount > 0 ? "true" : "false");
			ImGui::Text("Camera Input Enabled: %s", playScene->cameraInputEnabled_ ? "true" : "false");
		}
		ImGui::End();

		ImGui::Begin("Visibility & Cull");
		ImGui::Checkbox("Terrain", &playScene->showTerrain_);
		ImGui::Checkbox("Sphere", &playScene->showSphere_);
		ImGui::Checkbox("Plane", &playScene->showPlane_);
		ImGui::Checkbox("Sprite (2D)", &playScene->showSprite_);
		ImGui::Checkbox("Particles", &playScene->showParticles_);
		ImGui::Checkbox("Animated Model", &playScene->showAnimModel_);
		ImGui::Checkbox("Skeleton Debug", &playScene->showSkeleton_);
		ImGui::Checkbox("Debug Grid", &playScene->showDebugGrid_);
		ImGui::Separator();
		static const char* cullItems[] = { "None (両面)", "Front (前面削除)", "Back (背面削除)" };
		ImGui::Combo("Cull Mode", &playScene->cullMode_, cullItems, 3);
		ImGui::End();

		ImGui::Begin("Camera Control");
		ImGui::DragFloat3("Camera Position", &playScene->cameraPos_.x, 0.1f);
		ImGui::DragFloat3("Camera Rotation", &playScene->cameraRot_.x, 0.01f);
		ImGui::DragFloat("Move Speed", &playScene->cameraMoveSpeed_, 0.1f, 0.0f, 100.0f);
		ImGui::DragFloat("Rotate Speed", &playScene->cameraRotateSpeed_, 0.01f, 0.0f, 10.0f);
		playScene->cameraMoveSpeed_ = (std::max)(playScene->cameraMoveSpeed_, 0.0f);
		playScene->cameraRotateSpeed_ = (std::max)(playScene->cameraRotateSpeed_, 0.0f);
		if (ImGui::Button("Reset Camera")) {
			playScene->ResetCamera();
		}
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

		// ★スポットライト
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

		if (ImGui::CollapsingHeader("Animation Control", ImGuiTreeNodeFlags_DefaultOpen)) {
			// 配布データを含むアニメーションモデルの動的切り替えUI
			static const char* animModelNames[] = { "AnimatedCube (仮)", "simpleSkin (配布データ)", "human/walk (配布データ)", "human/sneakWalk (配布データ)" };
			if (ImGui::Combo("Model Select", &playScene->currentAnimModelIdx_, animModelNames, 4)) {
				playScene->ChangeAnimationModel(playScene->currentAnimModelIdx_);
			}
			ImGui::Checkbox("Show Model", &playScene->showAnimModel_);
			if (Object3d* animationObject = playScene->GetAnimationObject()) {
				ImGui::Checkbox("Play Animation", &animationObject->GetIsAnimationPlaying());
				ImGui::SliderFloat("Speed", &animationObject->GetAnimationSpeed(), -5.0f, 5.0f);
				float duration = animationObject->GetAnimation().duration;
				if (duration > 0.0f) {
					ImGui::SliderFloat("Time", &animationObject->GetAnimationTime(), 0.0f, duration);
				}
			}
		}

		// === Cylinderパラメータ（資料「Cylinderの拡張」対応） ===
		if (ImGui::CollapsingHeader("Cylinder (Effect)", ImGuiTreeNodeFlags_DefaultOpen)) {
			bool changed = false;
			// 上面の半径（0にすると円錐になる）
			changed |= ImGui::SliderFloat("Top Radius", &playScene->cylTopRadius_, 0.0f, 5.0f);
			// 下面の半径
			changed |= ImGui::SliderFloat("Bottom Radius", &playScene->cylBottomRadius_, 0.0f, 5.0f);
			// 高さ
			changed |= ImGui::SliderFloat("Cyl Height", &playScene->cylHeight_, 0.1f, 10.0f);
			// 円周方向の分割数
			changed |= ImGui::SliderInt("Segments", &playScene->cylSegments_, 3, 64);
			// 高さ方向の分割数（頂点カラーグラデーション用）
			changed |= ImGui::SliderInt("Vert Divisions", &playScene->cylVertDivisions_, 1, 16);
			// パラメータが変わったらメッシュを再生成
			if (changed) {
				playScene->RebuildCylinder();
			}
		}

		if (Object3d* animationObject = playScene->GetAnimationObject(); animationObject && animationObject->GetSkeleton()) {
			if (ImGui::CollapsingHeader("Skeleton Bones Control", ImGuiTreeNodeFlags_DefaultOpen)) {
				// 参照を変数として取得
				auto& skeletonOpt = animationObject->GetSkeleton();
				if (skeletonOpt.has_value()) {
					Skeleton& skeleton = skeletonOpt.value();

					// 1. ジョイント名の一覧を作ってコンボボックスにする
					static int selectedJointIdx = 0;
					std::vector<const char*> jointNames;
					for (const auto& joint : skeleton.joints) {
						jointNames.push_back(joint.name.c_str());
					}

					if (selectedJointIdx >= static_cast<int>(jointNames.size())) {
						selectedJointIdx = 0;
					}

					ImGui::Combo("Target Joint", &selectedJointIdx, jointNames.data(), static_cast<int>(jointNames.size()));

					ImGui::Separator();

					// 2. 選択されたジョイントのトランスフォームを直接いじる
					if (!skeleton.joints.empty()) {
						Joint& activeJoint = skeleton.joints[selectedJointIdx];

						ImGui::Text("Active: %s (Index: %d)", activeJoint.name.c_str(), activeJoint.index);
						ImGui::DragFloat3("Bone Translate", &activeJoint.transform.translate.x, 0.05f);
						ImGui::DragFloat3("Bone Scale", &activeJoint.transform.scale.x, 0.05f, 0.01f, 10.0f);

						// 回転をクォータニオンへ安全に反映させるための補助UI
						static Vector3 boneEulerDeg = { 0.0f, 0.0f, 0.0f };
						if (ImGui::DragFloat3("Bone Rotate (Euler)", &boneEulerDeg.x, 0.5f)) {
							// ここで必要に応じてオイラー角からクォータニオンを再構築して activeJoint.transform.rotate に代入します
						}
					}
				}
			}
		}
		ImGui::End();

		static SkinningDebugWindow skinningDebugWindow;
		skinningDebugWindow.Draw(playScene->GetAnimationObject());

		ImGui::Begin("Effect Control");

		ImGui::Text("Space Key: Emit");
		const char* pTypes[] = { "Spark (Manual)", "Ring (Model)", "Cylinder (Primitive)", "Combined", "Explosion (Emit)" };
		ImGui::Combo("Particle Mode", &playScene->activeParticleType_, pTypes, 5);

		ImGui::Separator();
		const char* gpuParticleModeItems[] = { "Off", "Agriculture Mode", "Particle Interaction Mode" };
		int gpuParticleModeIndex = static_cast<int>(playScene->gpuParticleDebugMode_);
		if (ImGui::Combo("GPUParticle Mode", &gpuParticleModeIndex, gpuParticleModeItems, _countof(gpuParticleModeItems))) {
			playScene->SetGPUParticleDebugMode(static_cast<GamePlayScene::GPUParticleDebugMode>(gpuParticleModeIndex));
		}

		if (playScene->gpuParticleDebugMode_ == GamePlayScene::GPUParticleDebugMode::Agriculture) {
			if (ImGui::CollapsingHeader("Agriculture Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
				ImGui::DragFloat3("Emit Position", &playScene->agricultureEmitPosition_.x, 0.1f);
				ImGui::SliderFloat("Particle Size", &playScene->agricultureParticleSize_, 0.02f, 1.0f);
				ImGui::SliderInt("Count", &playScene->agricultureParticleCount_, 1, 1024);

				if (ImGui::Button("Dirt Dust")) {
					playScene->EmitAgricultureParticle(GamePlayScene::AgricultureParticleType::DirtDust);
				}
				ImGui::SameLine();
				if (ImGui::Button("Water Splash")) {
					playScene->EmitAgricultureParticle(GamePlayScene::AgricultureParticleType::WaterSplash);
				}
				ImGui::SameLine();
				if (ImGui::Button("Harvest Sparkle")) {
					playScene->EmitAgricultureParticle(GamePlayScene::AgricultureParticleType::HarvestSparkle);
				}

				ImGui::Checkbox("Show Key Guide", &playScene->agricultureShowKeyGuide_);
				if (playScene->agricultureShowKeyGuide_) {
					ImGui::TextUnformatted("1: Dirt Dust  2: Water Splash  3: Harvest Sparkle");
					ImGui::TextUnformatted("4: Pollen / Spore  5: Bug Swarm");
				}
			}
		} else if (playScene->gpuParticleDebugMode_ == GamePlayScene::GPUParticleDebugMode::Interaction) {
			if (ImGui::CollapsingHeader("Interaction Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
				bool changed = false;
				changed |= ImGui::SliderInt("Particle Grid Count", &playScene->interactionGridCount_, 2, 10);
				changed |= ImGui::SliderFloat("Particle Size", &playScene->interactionParticleSize_, 0.02f, 0.05f);
				ImGui::SliderFloat("Brush Radius", &playScene->interactionBrushRadius_, 0.1f, 10.0f);
				ImGui::SliderFloat("Brush Strength", &playScene->interactionBrushStrength_, 0.0f, 10.0f);
				ImGui::SliderFloat("Interaction Damping", &playScene->interactionDamping_, 0.80f, 0.995f);
				playScene->interactionGridCount_ = std::clamp(playScene->interactionGridCount_, 2, 10);
				playScene->interactionDamping_ = std::clamp(playScene->interactionDamping_, 0.80f, 0.995f);
				playScene->interactionParticleCount_ = playScene->CalculateInteractionParticleCount();
				ImGui::Text("Current Particle Count: %u", playScene->interactionParticleCount_);
				ImGui::Text("Brush Position: %.2f, %.2f, %.2f",
					playScene->interactionBrushPosition_.x,
					playScene->interactionBrushPosition_.y,
					playScene->interactionBrushPosition_.z);
				if (changed) {
					playScene->interactionResetRequested_ = true;
				}
				if (ImGui::Button("Reset Grid")) {
					Framework::GetInstance()->GetParticleManager()->ResetGPUParticles();
					playScene->interactionResetRequested_ = true;
				}
				ImGui::TextUnformatted("Left Click on Game Viewport: Push");
				ImGui::TextUnformatted("Shift + Left Click: Pull");
			}
		}

		// === discard しきい値（資料スライド10対応） ===
		// この値以下のアルファを持つピクセルが棄却される
		// 0.0 = 完全透明のみ棄却（従来動作）
		// 0.5 = 半透明以下を棄却（くっきりした輪郭になる）
		static float alphaRef = 0.0f;
		if (ImGui::SliderFloat("Alpha Discard", &alphaRef, 0.0f, 1.0f)) {
			Framework::GetInstance()->GetParticleManager()->SetAlphaReference(alphaRef);
		}

		ImGui::Separator();
		if (ImGui::Button("Clear All Particles")) {
			Framework::GetInstance()->GetParticleManager()->ClearAll();
		}

		ImGui::End();

		DrawGameplayEffectImGui();

		// Fullscreen post effect selection.
		ImGui::Begin("Fullscreen PostEffect");
		const char* postEffectItems[] = {
			"None / Copy",
			"Grayscale",
			"Sepia",
			"Blur",
			"Bloom",
			"BoxFilter 3x3",
			"BoxFilter 5x5",
			"Radial Blur",
			"Dissolve",
			"Outline Luminance",
			"Outline Depth",
			"Outline Normal",
			"Outline Depth + Normal",
			"Vignette",
			"RandomNoise",
			"HSV Filter",
		};
		bool chainModeEnabled = postEffectManager_ && postEffectManager_->IsChainModeEnabled();
		if (postEffectManager_) {
			if (ImGui::Checkbox("PostEffect Chain Mode", &chainModeEnabled)) {
				postEffectManager_->SetChainModeEnabled(chainModeEnabled);
			}
		}
		if (!chainModeEnabled) {
			ImGui::Combo("Fullscreen Effect", &fullscreenPostEffectIndex_, postEffectItems, _countof(postEffectItems));
			fullscreenPostEffectIndex_ = std::clamp(fullscreenPostEffectIndex_, 0, static_cast<int>(_countof(postEffectItems)) - 1);
		} else {
			ImGui::TextUnformatted("Fullscreen Effect selector is ignored in Chain Mode.");
		}
		if (postEffectManager_) {
			if (chainModeEnabled) {
				ImGui::TextUnformatted("Chain Order");
				for (size_t i = 0; i < postEffectManager_->GetChainPassCount(); ++i) {
					bool passEnabled = postEffectManager_->IsChainPassEnabled(i);
					if (ImGui::Checkbox(postEffectManager_->GetChainPassName(i), &passEnabled)) {
						postEffectManager_->SetChainPassEnabled(i, passEnabled);
					}
				}
			}
			if (ImGui::CollapsingHeader("PostEffect Chain Debug", ImGuiTreeNodeFlags_DefaultOpen)) {
				ImGui::Text("ChainMode: %s", chainModeEnabled ? "ON" : "OFF");
				ImGui::Text("Enabled Passes: %zu / %zu",
					postEffectManager_->GetEnabledChainPassCount(),
					postEffectManager_->GetChainPassCount());
				for (size_t i = 0; i < postEffectManager_->GetChainPassCount(); ++i) {
					ImGui::Text("%zu. %s: %s",
						i + 1,
						postEffectManager_->GetChainPassName(i),
						(postEffectManager_->IsChainPassEnabled(i) || postEffectManager_->IsChainPassRuntimeEnabled(i)) ? "ON" : "OFF");
					if (postEffectManager_->IsChainPassRuntimeEnabled(i)) {
						ImGui::SameLine();
						ImGui::TextUnformatted("(Runtime)");
					}
				}
				ImGui::TextUnformatted("GammaCorrection: Fixed Last");
				ImGui::TextUnformatted("Copy: Fallback when no chain pass is enabled");
				ImGui::TextUnformatted("Viewport Source: FinalDisplayTexture");
				ImGui::TextUnformatted("Swapchain Source: FinalDisplayTexture");
			}
		}
		ImGui::Separator();
		ImGui::SliderFloat("Vignette Scale", &fullscreenVignetteScale_, 0.0f, 64.0f);
		ImGui::SliderFloat("Vignette Power", &fullscreenVignettePower_, 0.01f, 8.0f);
		ImGui::SliderFloat("Vignette Intensity", &fullscreenVignetteIntensity_, 0.0f, 1.0f);
		ImGui::Separator();
		const char* randomNoiseModeItems[] = { "WhiteNoiseOnly", "MultiplyScene" };
		ImGui::Combo("Random Noise Mode", &fullscreenRandomNoiseMode_, randomNoiseModeItems, _countof(randomNoiseModeItems));
		fullscreenRandomNoiseMode_ = std::clamp(fullscreenRandomNoiseMode_, 0, static_cast<int>(_countof(randomNoiseModeItems)) - 1);
		ImGui::SliderFloat("Random Noise Strength", &fullscreenRandomNoiseStrength_, 0.0f, 1.0f);
		ImGui::SliderFloat("Random Noise Scale", &fullscreenRandomNoiseScale_, 1.0f, 2000.0f);
		ImGui::Checkbox("Random Noise Animate", &fullscreenRandomNoiseAnimate_);
		ImGui::SliderFloat("Random Noise Time Speed", &fullscreenRandomNoiseTimeSpeed_, 0.0f, 10.0f);
		ImGui::Separator();
		ImGui::SliderFloat("HSV Hue", &fullscreenHSVHue_, -1.0f, 1.0f);
		ImGui::SliderFloat("HSV Saturation", &fullscreenHSVSaturation_, -1.0f, 1.0f);
		ImGui::SliderFloat("HSV Value", &fullscreenHSVValue_, -1.0f, 1.0f);
		if (ImGui::Button("Reset HSV")) {
			fullscreenHSVHue_ = 0.0f;
			fullscreenHSVSaturation_ = 0.0f;
			fullscreenHSVValue_ = 0.0f;
		}
		ImGui::SliderFloat("Grayscale Amount", &fullscreenGrayscaleIntensity_, 0.0f, 1.0f);
		ImGui::SliderFloat("Sepia Amount", &fullscreenSepiaIntensity_, 0.0f, 1.0f);
		ImGui::SliderFloat("Blur Strength", &fullscreenBlurStrength_, 0.0f, 16.0f);
		ImGui::Separator();
		ImGui::SliderFloat("Bloom Threshold", &fullscreenBloomThreshold_, 0.0f, 1.0f);
		ImGui::SliderFloat("Bloom Intensity", &fullscreenBloomIntensity_, 0.0f, 8.0f);
		ImGui::SliderFloat("Bloom Radius", &fullscreenBloomRadius_, 0.0f, 32.0f);
		ImGui::SliderFloat("Bloom Soft Knee", &fullscreenBloomSoftKnee_, 0.001f, 1.0f);
		ImGui::Separator();
		ImGui::SliderFloat("Radial Center X", &fullscreenRadialBlurCenter_.x, 0.0f, 1.0f);
		ImGui::SliderFloat("Radial Center Y", &fullscreenRadialBlurCenter_.y, 0.0f, 1.0f);
		ImGui::SliderFloat("Radial Blur Width", &fullscreenRadialBlurWidth_, 0.0f, 0.1f);
		ImGui::SliderFloat("Radial Blur Intensity", &fullscreenRadialBlurIntensity_, 0.0f, 1.0f);
		ImGui::SliderInt("Radial Sample Count", &fullscreenRadialBlurSampleCount_, 1, 32);
		ImGui::Separator();
		if (!noiseNames_.empty()) {
			selectedNoiseIndex_ = std::clamp(selectedNoiseIndex_, 0, static_cast<int>(noiseNames_.size()) - 1);
			std::vector<const char*> noiseNameItems;
			noiseNameItems.reserve(noiseNames_.size());
			for (const std::string& noiseName : noiseNames_) {
				noiseNameItems.push_back(noiseName.c_str());
			}
			ImGui::Combo("Noise Texture", &selectedNoiseIndex_, noiseNameItems.data(), static_cast<int>(noiseNameItems.size()));
			selectedNoiseIndex_ = std::clamp(selectedNoiseIndex_, 0, static_cast<int>(noiseNames_.size()) - 1);
		}
		ImGui::SliderFloat("Dissolve Threshold", &fullscreenDissolveThreshold_, 0.0f, 1.0f);
		ImGui::SliderFloat("Dissolve Edge Width", &fullscreenDissolveEdgeWidth_, 0.001f, 0.2f);
		ImGui::SliderFloat("Dissolve Edge Intensity", &fullscreenDissolveEdgeIntensity_, 0.0f, 5.0f);
		ImGui::ColorEdit3("Dissolve Edge Color", &fullscreenDissolveEdgeColor_.x);
		ImGui::Checkbox("Dissolve Enable Edge", &fullscreenDissolveEnableEdge_);
		ImGui::Separator();
		ImGui::SliderFloat("Outline Threshold", &fullscreenOutlineThreshold_, 0.0f, 1.0f);
		ImGui::SliderFloat("Outline Intensity", &fullscreenOutlineIntensity_, 0.0f, 8.0f);
		ImGui::SliderFloat("Outline Thickness", &fullscreenOutlineThickness_, 0.0f, 8.0f);
		ImGui::SliderFloat("Depth Outline Threshold", &fullscreenDepthOutlineThreshold_, 0.0f, 0.1f, "%.5f");
		ImGui::SliderFloat("Depth Outline Intensity", &fullscreenDepthOutlineIntensity_, 0.0f, 8.0f);
		ImGui::SliderFloat("Depth Outline Thickness", &fullscreenDepthOutlineThickness_, 1.0f, 8.0f);
		ImGui::Checkbox("Depth Linearize", &fullscreenDepthOutlineLinearize_);
		ImGui::SliderFloat("Normal Outline Threshold", &fullscreenNormalOutlineThreshold_, 0.0f, 4.0f);
		ImGui::SliderFloat("Normal Outline Intensity", &fullscreenNormalOutlineIntensity_, 0.0f, 8.0f);
		ImGui::SliderFloat("Normal Outline Thickness", &fullscreenNormalOutlineThickness_, 1.0f, 8.0f);
		if (ImGui::Button("Reset PostEffect Params")) {
			fullscreenGrayscaleIntensity_ = 1.0f;
			fullscreenSepiaIntensity_ = 1.0f;
			fullscreenBlurStrength_ = 4.0f;
			fullscreenBloomThreshold_ = 0.65f;
			fullscreenBloomIntensity_ = 1.5f;
			fullscreenBloomRadius_ = 8.0f;
			fullscreenBloomSoftKnee_ = 0.2f;
			fullscreenRadialBlurCenter_ = { 0.5f, 0.5f };
			fullscreenRadialBlurWidth_ = 0.01f;
			fullscreenRadialBlurIntensity_ = 1.0f;
			fullscreenRadialBlurSampleCount_ = 10;
			fullscreenDissolveThreshold_ = 0.5f;
			fullscreenDissolveEdgeWidth_ = 0.03f;
			fullscreenDissolveEdgeIntensity_ = 1.0f;
			fullscreenDissolveEnableEdge_ = true;
			fullscreenDissolveEdgeColor_ = { 1.0f, 0.4f, 0.3f };
			fullscreenOutlineThreshold_ = 0.15f;
			fullscreenOutlineIntensity_ = 1.0f;
			fullscreenOutlineThickness_ = 1.0f;
			fullscreenDepthOutlineThreshold_ = 0.001f;
			fullscreenDepthOutlineIntensity_ = 1.0f;
			fullscreenDepthOutlineThickness_ = 1.0f;
			fullscreenDepthOutlineLinearize_ = true;
			fullscreenNormalOutlineThreshold_ = 0.25f;
			fullscreenNormalOutlineIntensity_ = 1.0f;
			fullscreenNormalOutlineThickness_ = 1.0f;
			fullscreenVignetteScale_ = 16.0f;
			fullscreenVignettePower_ = 0.8f;
			fullscreenVignetteIntensity_ = 1.0f;
			fullscreenRandomNoiseTime_ = 0.0f;
			fullscreenRandomNoiseStrength_ = 0.2f;
			fullscreenRandomNoiseScale_ = 800.0f;
			fullscreenRandomNoiseTimeSpeed_ = 1.0f;
			fullscreenRandomNoiseMode_ = 1;
			fullscreenRandomNoiseAnimate_ = true;
			fullscreenHSVHue_ = 0.0f;
			fullscreenHSVSaturation_ = 0.0f;
			fullscreenHSVValue_ = 0.0f;
		}
		ImGui::End();
	}

	SceneManager::GetInstance()->Update();
	if (playScene) {
		FieldActionFeedbackType actionType = FieldActionFeedbackType::None;
		Vector3 actionPosition{};
		if (FieldManager* fieldManager = playScene->GetFieldManager();
			fieldManager && fieldManager->ConsumeActionFeedbackEvent(actionType, actionPosition)) {
			ShowFieldActionMessage(actionType, actionPosition);
		}

		Vector3 harvestPosition{};
		int32_t harvestPrice = 0;
		bool rareHarvest = false;
		if (playScene->ConsumeFieldHarvestEvent(harvestPosition, harvestPrice, rareHarvest)) {
			PlayHarvestEffect(harvestPosition, harvestPrice);
			if (rareHarvest && particleManager_) {
				particleManager_->PlayCropBurst(harvestPosition, CropBurstLevel::Rare);
			}
			if (rareHarvest && !autoDemoSequenceActive_) {
				PlayDigitalImpactEffect(harvestPosition);
			}
		}
	}
	ImGuiManager::GetInstance()->End();
}

void Game::Draw() {
	dxCommon_->PreDraw();
	srvManager_->PreDraw();

	// Draw the scene into the offscreen RenderTexture.
	SceneManager::GetInstance()->Draw();
	DrawGameplayEffectSprites();
	DrawGameplayHud(dynamic_cast<GamePlayScene*>(SceneManager::GetInstance()->GetCurrentScene()));

	// Fullscreen post effect keeps linear values in PostEffectResultTexture.
	// Gamma correction is applied once into FinalDisplayTexture for swapchain
	// presentation and the ImGui Game Viewport.
	const GameplayEffectManager::ScreenPostEffectModifier gameplayModifier =
		gameplayEffectManager_ ? gameplayEffectManager_->GetScreenPostEffectModifier() : GameplayEffectManager::ScreenPostEffectModifier{};
	DirectXCommon::FullscreenPostEffectParameter postEffectParameter{};
	postEffectParameter.grayscaleIntensity = fullscreenGrayscaleIntensity_;
	postEffectParameter.sepiaIntensity = fullscreenSepiaIntensity_;
	postEffectParameter.blurStrength = fullscreenBlurStrength_;
	postEffectParameter.bloomThreshold = fullscreenBloomThreshold_;
	postEffectParameter.bloomIntensity = fullscreenBloomIntensity_;
	postEffectParameter.bloomRadius = fullscreenBloomRadius_;
	postEffectParameter.bloomSoftKnee = fullscreenBloomSoftKnee_;
	postEffectParameter.outlineThreshold = fullscreenOutlineThreshold_;
	postEffectParameter.outlineIntensity = fullscreenOutlineIntensity_;
	postEffectParameter.outlineThickness = fullscreenOutlineThickness_;
	postEffectParameter.depthOutlineThreshold = fullscreenDepthOutlineThreshold_;
	postEffectParameter.depthOutlineIntensity = fullscreenDepthOutlineIntensity_;
	postEffectParameter.depthOutlineThickness = fullscreenDepthOutlineThickness_;
	postEffectParameter.depthOutlineLinearize = fullscreenDepthOutlineLinearize_ ? 1.0f : 0.0f;
	postEffectParameter.normalOutlineThreshold = fullscreenNormalOutlineThreshold_;
	postEffectParameter.normalOutlineIntensity = fullscreenNormalOutlineIntensity_;
	postEffectParameter.normalOutlineThickness = fullscreenNormalOutlineThickness_;
	if (BaseScene* currentScene = SceneManager::GetInstance()->GetCurrentScene()) {
		if (GamePlayScene* playScene = dynamic_cast<GamePlayScene*>(currentScene)) {
			if (Camera* camera = playScene->GetCamera()) {
				postEffectParameter.depthOutlineNearClip = camera->GetNearClip();
				postEffectParameter.depthOutlineFarClip = camera->GetFarClip();
			}
		}
	}
	dxCommon_->SetFullscreenPostEffectParameter(postEffectParameter);
	DirectXCommon::VignetteParamForGPU vignetteParameter{};
	vignetteParameter.scale = (std::max)(fullscreenVignetteScale_ + gameplayModifier.vignetteScaleAdd, 0.0f);
	vignetteParameter.power = (std::max)(fullscreenVignettePower_ + gameplayModifier.vignettePowerAdd, 0.01f);
	vignetteParameter.intensity = std::clamp(
		fullscreenVignetteIntensity_ + gameplayModifier.vignetteIntensityAdd,
		0.0f,
		1.0f);
	dxCommon_->SetVignetteParameter(vignetteParameter);
	DirectXCommon::RadialBlurParamForGPU radialBlurParameter{};
	radialBlurParameter.center = gameplayModifier.forceRadialBlur ? gameplayModifier.radialCenter : fullscreenRadialBlurCenter_;
	radialBlurParameter.blurWidth = std::clamp(
		fullscreenRadialBlurWidth_ + gameplayModifier.radialBlurWidthAdd,
		0.0f,
		0.1f);
	radialBlurParameter.intensity = std::clamp(
		fullscreenRadialBlurIntensity_ + gameplayModifier.radialBlurIntensityAdd,
		0.0f,
		1.0f);
	radialBlurParameter.sampleCount = (std::max)(fullscreenRadialBlurSampleCount_, gameplayModifier.radialSampleCountMin);
	dxCommon_->SetRadialBlurParameter(radialBlurParameter);
	DirectXCommon::DissolveParamForGPU dissolveParameter{};
	dissolveParameter.threshold = fullscreenDissolveThreshold_;
	dissolveParameter.edgeWidth = fullscreenDissolveEdgeWidth_;
	dissolveParameter.edgeIntensity = fullscreenDissolveEdgeIntensity_;
	dissolveParameter.enableEdge = fullscreenDissolveEnableEdge_ ? 1.0f : 0.0f;
	dissolveParameter.edgeColor = fullscreenDissolveEdgeColor_;
	dxCommon_->SetDissolveParameter(dissolveParameter);
	DirectXCommon::RandomNoiseParamForGPU randomNoiseParameter{};
	randomNoiseParameter.time = fullscreenRandomNoiseTime_;
	randomNoiseParameter.strength = std::clamp(
		fullscreenRandomNoiseStrength_ + gameplayModifier.randomNoiseStrengthAdd,
		0.0f,
		1.0f);
	randomNoiseParameter.scale = gameplayModifier.forceRandomNoise ? gameplayModifier.randomNoiseScale : fullscreenRandomNoiseScale_;
	randomNoiseParameter.mode = static_cast<float>(gameplayModifier.forceRandomNoise ? gameplayModifier.randomNoiseMode : fullscreenRandomNoiseMode_);
	randomNoiseParameter.animate = (fullscreenRandomNoiseAnimate_ || gameplayModifier.randomNoiseAnimate) ? 1.0f : 0.0f;
	dxCommon_->SetRandomNoiseParameter(randomNoiseParameter);
	DirectXCommon::HSVFilterParamForGPU hsvFilterParameter{};
	hsvFilterParameter.hue = fullscreenHSVHue_;
	hsvFilterParameter.saturation = std::clamp(
		fullscreenHSVSaturation_ + gameplayModifier.hsvSaturationAdd,
		-1.0f,
		1.0f);
	hsvFilterParameter.value = std::clamp(
		fullscreenHSVValue_ + gameplayModifier.hsvValueAdd,
		-1.0f,
		1.0f);
	dxCommon_->SetHSVFilterParameter(hsvFilterParameter);

	srvManager_->PreDraw();
	DirectXCommon::FullscreenPostEffectType postEffectType =
		static_cast<DirectXCommon::FullscreenPostEffectType>(fullscreenPostEffectIndex_);
	if (postEffectManager_) {
		postEffectManager_->ClearRuntimeChainOverrides();
		if (gameplayModifier.forceChainMode) {
			postEffectManager_->SetRuntimeChainModeEnabled(true);
			postEffectManager_->SetRuntimeChainPassEnabled(DirectXCommon::FullscreenPostEffectType::HSVFilter, gameplayModifier.forceHSVFilter);
			postEffectManager_->SetRuntimeChainPassEnabled(DirectXCommon::FullscreenPostEffectType::Vignette, gameplayModifier.forceVignette);
			postEffectManager_->SetRuntimeChainPassEnabled(DirectXCommon::FullscreenPostEffectType::RadialBlur, gameplayModifier.forceRadialBlur);
			postEffectManager_->SetRuntimeChainPassEnabled(DirectXCommon::FullscreenPostEffectType::RandomNoise, gameplayModifier.forceRandomNoise);
			if (!postEffectManager_->IsChainModeEnabled() &&
				postEffectType != DirectXCommon::FullscreenPostEffectType::Copy &&
				postEffectType != DirectXCommon::FullscreenPostEffectType::LinearToSRGB) {
				postEffectManager_->SetRuntimeChainPassEnabled(postEffectType, true);
			}
		}
	}
	D3D12_GPU_DESCRIPTOR_HANDLE auxiliarySrvHandle{};
	const bool chainModeEnabled = postEffectManager_ && postEffectManager_->IsChainExecutionEnabled();
	const bool needsNoiseSrv =
		chainModeEnabled ||
		postEffectType == DirectXCommon::FullscreenPostEffectType::Dissolve;
	if (needsNoiseSrv && !noiseSrvIndices_.empty()) {
		selectedNoiseIndex_ = std::clamp(selectedNoiseIndex_, 0, static_cast<int>(noiseSrvIndices_.size()) - 1);
		auxiliarySrvHandle = srvManager_->GetGPUDescriptorHandle(noiseSrvIndices_[selectedNoiseIndex_]);
	}
	postEffectManager_->Execute(postEffectType, auxiliarySrvHandle);

	// Render ImGui last on the swapchain, then present.
	ImGuiManager::GetInstance()->Draw();
	dxCommon_->RestoreRenderTextureToRenderTarget();
	dxCommon_->PostDraw();
}
