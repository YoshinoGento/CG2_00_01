#include "Game.h"

#include "3d/Camera.h"
#include "base/DirectXCommon.h"
#include "base/FrameClock.h"
#include "base/ImGuiManager.h"
#include "base/Logger.h"
#include "base/SrvManager.h"
#include "demo/PostEffectSubmissionDemo.h"
#include "demo/PostEffectSubmissionHUD.h"
#ifdef USE_IMGUI
#include "editor/EditorShell.h"
#endif
#include "effect/PostEffectSystem.h"
#include "FloatingTextSystem.h"
#include "GameplayEffectManager.h"
#include "2d/RuntimeTextTextureGenerator.h"
#include "2d/Sprite.h"
#include "3d/SkyboxManager.h"
#include "scene/GamePlayScene.h"
#include "scene/SceneFactory.h"
#include "scene/SceneManager.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>

Vector2 Game::mousePosInViewport_ = { 0.0f, 0.0f };

namespace {
constexpr float kVirtualScreenWidth = 1280.0f;
constexpr float kVirtualScreenHeight = 720.0f;

size_t GetFieldStateHudIndex(FieldState state)
{
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

Vector4 GetFieldStateHudColor(FieldState state)
{
	switch (state) {
	case FieldState::Empty:
		return { 0.90f, 0.90f, 0.86f, 1.0f };
	case FieldState::Tilled:
		return { 0.82f, 0.58f, 0.34f, 1.0f };
	case FieldState::Watered:
		return { 0.45f, 0.78f, 1.0f, 1.0f };
	case FieldState::Planted:
		return { 0.50f, 0.95f, 0.48f, 1.0f };
	case FieldState::ReadyToHarvest:
		return { 1.0f, 0.86f, 0.24f, 1.0f };
	default:
		return { 1.0f, 1.0f, 1.0f, 1.0f };
	}
}

Vector4 GetGrowthBarColor(float growth)
{
	const float t = std::clamp(growth, 0.0f, 1.0f);
	return { 0.35f + 0.45f * t, 0.72f + 0.20f * t, 0.25f, 0.95f };
}

Vector4 GetMoistureBarColor(float moisture)
{
	const float t = std::clamp(moisture, 0.0f, 1.0f);
	return { 0.20f + 0.20f * t, 0.45f + 0.35f * t, 0.85f + 0.15f * t, 0.95f };
}

int GetActionMessageIndex(FieldActionFeedbackType type)
{
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

Game::Game() = default;
Game::~Game() = default;

void Game::Initialize() {
	Framework::Initialize();

	postEffectSystem_ = std::make_unique<PostEffectSystem>();
	if (!postEffectSystem_->Initialize(dxCommon_.get(), srvManager_.get())) {
		Logger::Log("Game::Initialize failed to initialize PostEffectSystem.");
		assert(false && "PostEffectSystem initialization failed");
	}
	InitializeRuntimeTextTextures();
	InitializeGameplayHud();
	gameplayEffectManager_ = std::make_unique<GameplayEffectManager>();
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

#ifdef USE_IMGUI
	editorShell_ = std::make_unique<EditorShell>();
	editorShell_->Initialize();
#else
	postEffectSubmissionDemo_ = std::make_unique<PostEffectSubmissionDemo>();
	postEffectSubmissionDemo_->Initialize(*postEffectSystem_, *winApp_);
	postEffectSubmissionHud_ = std::make_unique<PostEffectSubmissionHUD>();
	if (!postEffectSubmissionHud_->Initialize(spriteCommon_.get())) {
		Logger::Log("Game::Initialize failed to initialize PostEffectSubmissionHUD.");
		assert(false && "PostEffectSubmissionHUD initialization failed");
	}
#endif

	sceneFactory_ = std::make_unique<SceneFactory>();
	SceneManager::GetInstance()->SetSceneFactory(sceneFactory_.get());
#ifdef USE_IMGUI
	SceneManager::GetInstance()->ChangeScene("TITLE");
#else
	SceneManager::GetInstance()->ChangeScene("GAMEPLAY");
#endif
}

void Game::Finalize() {
#ifdef USE_IMGUI
	if (editorShell_) {
		editorShell_->Finalize();
		editorShell_.reset();
	}
#endif

	postEffectSubmissionHud_.reset();
	postEffectSubmissionDemo_.reset();
	SceneManager::DeleteInstance();
	if (postEffectSystem_) {
		postEffectSystem_->Finalize();
		postEffectSystem_.reset();
	}
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

void Game::DrawHudSprite(Sprite* sprite, const Vector2& position, const Vector2& size, const Vector4& color) {
	if (!sprite) {
		return;
	}
	sprite->SetPosition(position);
	sprite->SetSize(size);
	sprite->SetColor(color);
	sprite->Update();
	sprite->Draw();
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

#ifdef USE_IMGUI
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
#endif

void Game::Update() {
	Framework::Update();
	SceneManager* sceneManager = SceneManager::GetInstance();
	sceneManager->BeginFrame();

#ifndef USE_IMGUI
	if (postEffectSubmissionDemo_) {
		postEffectSubmissionDemo_->Update(
			frameClock_->GetFrameDeltaSeconds(),
			*input_,
			*postEffectSystem_,
			*winApp_);
		if (postEffectSubmissionHud_) {
			postEffectSubmissionHud_->SetViewData({
				.effectName = postEffectSubmissionDemo_->GetCurrentEffectName(),
				.showDissolveProgress = postEffectSubmissionDemo_->IsDissolveActive(),
				.dissolvePercent = postEffectSubmissionDemo_->GetDissolvePercent(),
			});
			postEffectSubmissionHud_->Update();
		}
	}
#endif

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

#ifndef USE_IMGUI
	// Submission text is display-space UI, so the demonstrated effect cannot hide its own name.
	if (postEffectSubmissionHud_) {
		spriteCommon_->PreDraw();
		postEffectSubmissionHud_->Draw();
	}
#endif

	// ImGui is composited on the swapchain after the gamma-corrected game image.
	ImGuiManager::GetInstance()->Draw();
	dxCommon_->RestoreRenderTextureToRenderTarget();
	dxCommon_->PostDraw();
}
