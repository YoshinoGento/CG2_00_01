#include "TitleScene.h"

#include "GameFlowState.h"
#include "SceneManager.h"
#include "2d/SpriteCommon.h"
#include "2d/Sprite.h"
#include "2d/TextureManager.h"
#include "3d/Camera.h"
#include "3d/Model.h"
#include "3d/ModelManager.h"
#include "3d/Object3d.h"
#include "3d/Object3dCommon.h"
#include "3d/Skybox.h"
#include "base/FrameClock.h"
#include "base/Framework.h"
#include "effect/ParticleManager.h"
#include "io/Input.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {
constexpr Vector4 kPrimaryColor{ 0.32f, 0.95f, 1.0f, 1.0f };
constexpr Vector4 kTextColor{ 0.88f, 0.94f, 0.98f, 1.0f };
constexpr Vector4 kAccentColor{ 1.0f, 0.82f, 0.24f, 1.0f };
constexpr Vector4 kResultPlayerColor{ 1.0f, 0.24f, 0.28f, 1.0f };
constexpr float kCharacterSpacing = -7.0f;
constexpr float kRankingRowTop = 160.0f;
constexpr float kRankingRowStep = 72.0f;
constexpr float kRankingTextScale = 1.65f;
constexpr float kRankingScoreModelScale = 0.28f;
constexpr float kRankingValueX = 710.0f;
constexpr float kRankingArrowLeftX = 795.0f;
constexpr float kRankingArrowWidth = 112.0f;
constexpr float kRankingArrowHeight = 56.0f;
constexpr float kHalfPi = 1.57079633f;
constexpr float kPi = 3.14159265f;
}

void TitleScene::Initialize()
{
	Framework* framework = Framework::GetInstance();
	SpriteCommon* spriteCommon = framework ? framework->GetSpriteCommon() : nullptr;
	GameFlowState::GetInstance().EnsureBgm(
		framework ? framework->GetAudio() : nullptr,
		page_ == Page::Ranking
			? GameFlowState::BgmTrack::Result
			: GameFlowState::BgmTrack::Title);
	const char* fontPath = page_ == Page::Instructions
		? "Resources/ui/font/japanese_instruction_font.json"
		: "Resources/ui/font/ascii_bitmap_font.json";
	if (!spriteCommon ||
		!font_.InitializeFromJson(spriteCommon, fontPath)) {
		return;
	}

	for (SpriteText& line : lines_) {
		line.Initialize(spriteCommon, &font_);
		line.SetCharacterSpacing(kCharacterSpacing);
	}
	for (SpriteText& valueText : rankingValueTexts_) {
		valueText.Initialize(spriteCommon, &font_);
		valueText.SetCharacterSpacing(kCharacterSpacing);
	}
	if (page_ == Page::Ranking && rankingStatusFont_.InitializeFromJson(
		spriteCommon, "Resources/ui/font/ranking_status_font.json")) {
		rankingStatusText_.Initialize(spriteCommon, &rankingStatusFont_);
		rankingStatusText_.SetCharacterSpacing(-8.0f);
		rankingStatusReady_ = true;
	}

	titleCamera_ = std::make_unique<Camera>();
	titleCamera_->SetTranslate({ 0.0f, 0.0f, -10.0f });
	titleCamera_->SetRotate({ 0.0f, 0.0f, 0.0f });
	titleCamera_->Update();

	// Menu scenes own a separate sky from gameplay so their look can be tuned independently.
	menuSkybox_ = std::make_unique<Skybox>();
	menuSkybox_->InitializeGradient(
		framework->GetDxCommon(),
		{ 0.30f, 0.36f, 0.52f, 1.0f },
		{ 0.52f, 0.41f, 0.31f, 1.0f });
	menuSkybox_->Update(titleCamera_.get());

	ModelManager* modelManager = framework->GetModelManager();
	if (modelManager) {
		constexpr const char* kKeyboardPromptModelPath = "title/Space.obj";
		constexpr const char* kGamepadPromptModelPath = "title/PushToB.obj";
		modelManager->LoadModel(kKeyboardPromptModelPath);
		modelManager->LoadModel(kGamepadPromptModelPath);
		keyboardTransitionModel_ = modelManager->GetModel(kKeyboardPromptModelPath);
		gamepadTransitionModel_ = modelManager->GetModel(kGamepadPromptModelPath);
		if (keyboardTransitionModel_) {
			keyboardTransitionModel_->LoadTextures();
		}
		if (gamepadTransitionModel_) {
			gamepadTransitionModel_->LoadTextures();
		}
		Model* initialPromptModel = keyboardTransitionModel_
			? keyboardTransitionModel_
			: gamepadTransitionModel_;
		if (initialPromptModel) {
			transitionKeyObject_ = std::make_unique<Object3d>();
			transitionKeyObject_->Initialize(framework->GetObject3dCommon());
			transitionKeyObject_->SetModel(initialPromptModel);
			transitionKeyObject_->SetTexture(
				TextureManager::GetInstance()->LoadTexture2D("Resources/human/white.png"));
			transitionKeyObject_->SetColor(kAccentColor);
			transitionKeyObject_->SetEnableLighting(false);
			transitionKeyObject_->SetCullMode(0);
		}

		if (page_ == Page::Instructions) {
			constexpr const char* kGuideModelPath = "title/guide.obj";
			modelManager->LoadModel(kGuideModelPath);
			Model* guideModel = modelManager->GetModel(kGuideModelPath);
			if (guideModel) {
				guideModel->LoadTextures();
				guideTitleObject_ = std::make_unique<Object3d>();
				guideTitleObject_->Initialize(framework->GetObject3dCommon());
				guideTitleObject_->SetModel(guideModel);
				guideTitleObject_->SetScale({ 0.40f, 0.40f, 0.40f });
				guideTitleObject_->SetPosition({ 0.044f, 1.35f, 0.0f });
				guideTitleObject_->SetRotation({ 0.0f, 3.14159265f, 0.0f });
				guideTitleObject_->SetColor(kPrimaryColor);
				guideTitleObject_->SetEnableLighting(false);
				guideTitleObject_->SetCullMode(0);
				guideTitleObject_->Update(titleCamera_.get(), 0.0f);
			}

			constexpr const char* kOperationModelPath = "title/Operation.obj";
			modelManager->LoadModel(kOperationModelPath);
			Model* operationModel = modelManager->GetModel(kOperationModelPath);
			if (operationModel) {
				operationModel->LoadTextures();
				operationGuideObject_ = std::make_unique<Object3d>();
				operationGuideObject_->Initialize(framework->GetObject3dCommon());
				operationGuideObject_->SetModel(operationModel);
				operationGuideObject_->SetScale({ 0.34f, 0.34f, 0.34f });
				operationGuideObject_->SetPosition({ 0.20f, -0.84f, 0.0f });
				operationGuideObject_->SetRotation({ 0.0f, 3.14159265f, 0.0f });
				operationGuideObject_->SetColor(kTextColor);
				operationGuideObject_->SetEnableLighting(false);
				operationGuideObject_->SetCullMode(0);
				operationGuideObject_->Update(titleCamera_.get(), 0.0f);
			}
		}

		if (page_ == Page::Ranking) {
			constexpr const char* kRankingModelPath = "title/Ranking.obj";
			modelManager->LoadModel(kRankingModelPath);
			Model* rankingModel = modelManager->GetModel(kRankingModelPath);
			if (rankingModel) {
				rankingModel->LoadTextures();
				rankingTitleObject_ = std::make_unique<Object3d>();
				rankingTitleObject_->Initialize(framework->GetObject3dCommon());
				rankingTitleObject_->SetModel(rankingModel);
				rankingTitleObject_->SetScale({ 0.60f, 0.60f, 0.60f });
				rankingTitleObject_->SetPosition({ 0.104f, 1.665f, 0.0f });
				rankingTitleObject_->SetRotation({ 0.0f, 3.14159265f, 0.0f });
				rankingTitleObject_->SetColor(kPrimaryColor);
				rankingTitleObject_->SetEnableLighting(false);
				rankingTitleObject_->SetCullMode(0);
				rankingTitleObject_->Update(titleCamera_.get(), 0.0f);
			}

			constexpr const char* kScoreModelPath = "title/Score.obj";
			modelManager->LoadModel(kScoreModelPath);
			Model* scoreModel = modelManager->GetModel(kScoreModelPath);
			if (scoreModel) {
				scoreModel->LoadTextures();
				const std::size_t scoreCount = GameFlowState::GetInstance().GetRankingCount();
				for (std::size_t index = 0;
					index < scoreCount && index < rankingScoreObjects_.size(); ++index) {
					auto& scoreObject = rankingScoreObjects_[index];
					scoreObject = std::make_unique<Object3d>();
					scoreObject->Initialize(framework->GetObject3dCommon());
					scoreObject->SetModel(scoreModel);
					scoreObject->SetScale({
						kRankingScoreModelScale,
						kRankingScoreModelScale,
						kRankingScoreModelScale });
					scoreObject->SetPosition(
						{ -0.08f, 1.14f - 0.462f * static_cast<float>(index), 0.0f });
					scoreObject->SetRotation({ 0.0f, 3.14159265f, 0.0f });
					scoreObject->SetColor(index == 0 ? kAccentColor : kTextColor);
					scoreObject->SetEnableLighting(false);
					scoreObject->SetCullMode(0);
					scoreObject->Update(titleCamera_.get(), 0.0f);
				}
			}
			InitializeRankingDecorations();
		}
	}

	if (page_ == Page::Title) {
		if (modelManager) {
			constexpr const char* kTitleModelPath = "title/Title.obj";
			modelManager->LoadModel(kTitleModelPath);
			Model* titleModel = modelManager->GetModel(kTitleModelPath);
			if (titleModel) {
				titleModel->LoadTextures();
				titleObject_ = std::make_unique<Object3d>();
				titleObject_->Initialize(framework->GetObject3dCommon());
				titleObject_->SetModel(titleModel);
				titleObject_->SetScale({ 0.72f, 0.72f, 0.72f });
				titleObject_->SetPosition({ 0.425f, 1.13f, 0.0f });
				titleObject_->SetRotation({ 0.0f, 3.14159265f, 0.0f });
				titleObject_->SetEnableLighting(false);
				titleObject_->SetCullMode(0);
				titleObject_->Update(titleCamera_.get(), 0.0f);
			}
		}
	} else if (page_ == Page::Instructions) {
		if (!guideTitleObject_) {
			SetLine(0, "遊び方", { 575.0f, 105.0f }, 0.78f, kPrimaryColor);
		}
		if (!operationGuideObject_) {
			SetLine(1, "左スティック または WASD：移動", { 365.0f, 215.0f }, 0.55f, kTextColor);
			SetLine(2, "LB・RB：回転", { 520.0f, 275.0f }, 0.55f, kTextColor);
			SetLine(3, "RT または Q：磁石を発射", { 430.0f, 335.0f }, 0.55f, kTextColor);
			SetLine(4, "磁石をゴールに入れる", { 465.0f, 395.0f }, 0.55f, kTextColor);
			SetLine(5, "MENU または ESC：ポーズ", { 420.0f, 455.0f }, 0.55f, kTextColor);
		}
	} else if (page_ == Page::StageSelect) {
		if (ParticleManager* particles = framework->GetParticleManager()) {
			particles->ClearAll();
			particles->CreateParticleGroup("StageSelectGlow",
				TextureManager::GetInstance()->LoadTexture2D("Resources/circle2.png"));
		}
		RefreshStageSelectLines();
	} else {
		if (!rankingTitleObject_) {
			SetLine(0, "RANKING", { 520.0f, 75.0f }, 1.65f, kPrimaryColor);
		}
		const auto& state = GameFlowState::GetInstance();
		const auto& ranking = state.GetRanking();
		for (std::size_t index = 0; index < GameFlowState::kRankingCapacity; ++index) {
			char rankBuffer[16]{};
			char scoreBuffer[32]{};
			if (index < state.GetRankingCount()) {
				std::snprintf(rankBuffer, sizeof(rankBuffer), "%zu", index + 1);
				std::snprintf(scoreBuffer, sizeof(scoreBuffer), "%zu", ranking[index]);
			} else {
				std::snprintf(rankBuffer, sizeof(rankBuffer), "%zu", index + 1);
				std::snprintf(scoreBuffer, sizeof(scoreBuffer), "---");
			}
			const float rowY = kRankingRowTop +
				kRankingRowStep * static_cast<float>(index);
			const Vector4 rowColor = index == 0 ? kAccentColor : kTextColor;
			SetLine(index + 1, rankBuffer, { 490.0f, rowY },
				kRankingTextScale, rowColor);
			rankingValueTexts_[index].SetText(scoreBuffer);
			rankingValueTexts_[index].SetPosition({ kRankingValueX, rowY });
			rankingValueTexts_[index].SetScale(kRankingTextScale);
			rankingValueTexts_[index].SetColor(rowColor);
			rankingValueTexts_[index].Update();
		}
		const auto currentRank = state.GetLastSubmittedRank();
		if (currentRank && *currentRank < state.GetRankingCount() &&
			*currentRank < GameFlowState::kRankingCapacity) {
			rankingCurrentArrowVisible_ = true;
		} else if (state.GetLastSubmittedScore() && rankingStatusReady_) {
			rankingStatusText_.SetText("ランク外");
			rankingStatusText_.SetPosition({ 850.0f, 330.0f });
			rankingStatusText_.SetScale(0.80f);
			rankingStatusText_.SetColor(kAccentColor);
			rankingStatusText_.Update();
			rankingStatusVisible_ = true;
		}
	}
	if (page_ == Page::Ranking) {
		rankingPresentationSystem_.Reset();
		UpdateRankingPresentation(0.0f);
	}
	Input* input = framework->GetInput();
	if (page_ == Page::StageSelect && input) {
		const Vector2 stick = input->GetLeftStick();
		stageSelectStickUpWasPressed_ = stick.y > 0.5f;
		stageSelectStickDownWasPressed_ = stick.y < -0.5f;
	}
	RefreshTransitionPrompt(
		input && input->GetLastActiveDevice() == InputDeviceType::Gamepad);
	uiReady_ = true;
}

void TitleScene::Finalize()
{
	rankingCurrentArrowSprite_.reset();
	for (auto& ball : rankingBallObjects_) { ball.reset(); }
	rankingPlayerObject_.reset();
	if (page_ == Page::StageSelect) {
		if (Framework* framework = Framework::GetInstance()) {
			if (ParticleManager* particles = framework->GetParticleManager()) {
				particles->ClearAll();
			}
		}
	}
	for (auto& scoreObject : rankingScoreObjects_) { scoreObject.reset(); }
	rankingTitleObject_.reset();
	operationGuideObject_.reset();
	guideTitleObject_.reset();
	transitionKeyObject_.reset();
	keyboardTransitionModel_ = nullptr;
	gamepadTransitionModel_ = nullptr;
	titleObject_.reset();
	menuSkybox_.reset();
	titleCamera_.reset();
	transitionPromptInitialized_ = false;
	transitionPromptUsesGamepad_ = false;
	transitionPromptModelVisible_ = false;
	rankingCurrentArrowVisible_ = false;
	rankingStatusReady_ = false;
	rankingStatusVisible_ = false;
	uiReady_ = false;
}

void TitleScene::Update()
{
	if (page_ == Page::StageSelect) {
		Framework* framework = Framework::GetInstance();
		const FrameClock* clock = framework ? framework->GetFrameClock() : nullptr;
		UpdateStageSelectVisuals(
			clock ? clock->GetFrameDeltaSeconds() : FrameClock::kDefaultFixedDeltaSeconds);
		if (ParticleManager* particles = framework ? framework->GetParticleManager() : nullptr) {
			particles->Update(titleCamera_.get(),
				clock ? clock->GetFrameDeltaSeconds() : FrameClock::kDefaultFixedDeltaSeconds);
		}
	}
	if (titleObject_ && titleCamera_) {
		titleObject_->Update(titleCamera_.get(), 0.0f);
	}
	if (transitionPromptModelVisible_ && transitionKeyObject_ && titleCamera_) {
		transitionKeyObject_->Update(titleCamera_.get(), 0.0f);
	}
	if (guideTitleObject_ && titleCamera_) {
		guideTitleObject_->Update(titleCamera_.get(), 0.0f);
	}
	if (operationGuideObject_ && titleCamera_) {
		operationGuideObject_->Update(titleCamera_.get(), 0.0f);
	}
	if (page_ == Page::Ranking) {
		FrameClock* frameClock = Framework::GetInstance()->GetFrameClock();
		UpdateRankingPresentation(
			frameClock ? frameClock->GetRealDeltaSeconds() : 0.0f);
	}
	if (menuSkybox_ && titleCamera_) {
		menuSkybox_->Update(titleCamera_.get());
	}
	Input* input = Framework::GetInstance()->GetInput();
	if (!input) { return; }
	const bool useGamepad =
		input->GetLastActiveDevice() == InputDeviceType::Gamepad;
	if (!transitionPromptInitialized_ || transitionPromptUsesGamepad_ != useGamepad) {
		RefreshTransitionPrompt(useGamepad);
	}
	if (page_ == Page::StageSelect) {
		const Vector2 stick = input->GetLeftStick();
		const bool stickUp = stick.y > 0.5f;
		const bool stickDown = stick.y < -0.5f;
		const bool moveUp = input->TriggerKey(InputKey::W) ||
			input->TriggerGamepadButton(InputGamepadButton::DPadUp) ||
			(stickUp && !stageSelectStickUpWasPressed_);
		const bool moveDown = input->TriggerKey(InputKey::S) ||
			input->TriggerGamepadButton(InputGamepadButton::DPadDown) ||
			(stickDown && !stageSelectStickDownWasPressed_);
		stageSelectStickUpWasPressed_ = stickUp;
		stageSelectStickDownWasPressed_ = stickDown;
		if (moveUp) {
			stageSelection_ = (stageSelection_ + 3) % 4;
			RefreshStageSelectLines();
			EmitStageSelectParticles(true);
		}
		if (moveDown) {
			stageSelection_ = (stageSelection_ + 1) % 4;
			RefreshStageSelectLines();
			EmitStageSelectParticles(true);
		}
	}
	const bool transitionRequested = input->TriggerKey(InputKey::Space) ||
		input->TriggerGamepadButton(InputGamepadButton::B);
	if (!transitionRequested) { return; }
	if (page_ == Page::Title) {
		SceneManager::GetInstance()->ChangeScene("INSTRUCTIONS");
	} else if (page_ == Page::Instructions) {
		SceneManager::GetInstance()->ChangeScene("STAGE_SELECT");
	} else if (page_ == Page::StageSelect) {
		constexpr const char* stageNames[] = {
			"stage_01", "stage_01", "stage_02", "stage_03" };
		GameFlowState::GetInstance().SetActiveStageSaveName(
			stageNames[stageSelection_]);
		SceneManager::GetInstance()->ChangeScene(
			stageSelection_ == 0 ? "TUTORIAL" : "MAGNET_PROTOTYPE");
	} else {
		SceneManager::GetInstance()->ChangeScene("STAGE_SELECT");
	}
}

void TitleScene::Draw()
{
	if (!uiReady_) { return; }
	if (menuSkybox_) { menuSkybox_->Draw(); }
	if (titleObject_ || transitionKeyObject_ || guideTitleObject_ ||
		operationGuideObject_ || rankingTitleObject_ || rankingScoreObjects_[0] ||
		rankingPlayerObject_) {
		Object3dCommon* objectCommon = Framework::GetInstance()->GetObject3dCommon();
		objectCommon->BeginObjectPass();
		if (titleObject_) { titleObject_->Draw(); }
		if (transitionPromptModelVisible_ && transitionKeyObject_) {
			transitionKeyObject_->Draw();
		}
		if (guideTitleObject_) { guideTitleObject_->Draw(); }
		if (operationGuideObject_) { operationGuideObject_->Draw(); }
		if (rankingTitleObject_) { rankingTitleObject_->Draw(); }
		for (auto& scoreObject : rankingScoreObjects_) {
			if (scoreObject) { scoreObject->Draw(); }
		}
		if (rankingPlayerObject_) { rankingPlayerObject_->Draw(); }
		for (auto& ball : rankingBallObjects_) {
			if (ball) { ball->Draw(); }
		}
		objectCommon->EndObjectPass();
	}
	if (page_ == Page::StageSelect) {
		if (ParticleManager* particles = Framework::GetInstance()->GetParticleManager()) {
			particles->Draw(false);
		}
	}
	Framework::GetInstance()->GetSpriteCommon()->PreDraw();
	for (std::size_t index = 0; index < lineCount_; ++index) {
		lines_[index].Draw();
	}
	if (page_ == Page::Ranking) {
		for (SpriteText& valueText : rankingValueTexts_) {
			valueText.Draw();
		}
		if (rankingStatusVisible_) {
			rankingStatusText_.Draw();
		}
		if (rankingCurrentArrowVisible_) {
			rankingCurrentArrowSprite_->Draw();
		}
	}
}

void TitleScene::InitializeRankingDecorations()
{
	Framework* framework = Framework::GetInstance();
	if (!framework || !framework->GetModelManager() ||
		!framework->GetObject3dCommon()) {
		return;
	}

	constexpr const char* kPlayerModelPath = "magnet/player/player.obj";
	constexpr const char* kSmallBallModelPath = "magnet/small_ball/SmallBall.obj";
	ModelManager* modelManager = framework->GetModelManager();
	modelManager->LoadModel(kPlayerModelPath);
	modelManager->LoadModel(kSmallBallModelPath);
	Model* playerModel = modelManager->GetModel(kPlayerModelPath);
	Model* smallBallModel = modelManager->GetModel(kSmallBallModelPath);
	if (!playerModel || !smallBallModel) {
		return;
	}

	const Texture2DHandle whiteTexture =
		TextureManager::GetInstance()->LoadTexture2D("Resources/human/white.png");
	const auto createSphere = [&](Model* model, const Vector4& color) {
		auto object = std::make_unique<Object3d>();
		object->Initialize(framework->GetObject3dCommon());
		object->SetModel(model);
		object->SetTexture(whiteTexture);
		object->SetColor(color);
		object->SetEnableLighting(false);
		object->SetCullMode(0);
		return object;
	};

	rankingPlayerObject_ = createSphere(playerModel, kResultPlayerColor);
	for (auto& ball : rankingBallObjects_) {
		ball = createSphere(smallBallModel, kPrimaryColor);
	}

	rankingCurrentArrowSprite_ = std::make_unique<Sprite>();
	if (!rankingCurrentArrowSprite_->Initialize(framework->GetSpriteCommon(),
		"Resources/ui/ranking_current_arrow.png")) {
		rankingCurrentArrowSprite_.reset();
		rankingCurrentArrowVisible_ = false;
	} else {
		rankingCurrentArrowSprite_->SetColor(kAccentColor);
	}
}

void TitleScene::UpdateRankingPresentation(float deltaSeconds)
{
	if (page_ != Page::Ranking || !titleCamera_) {
		return;
	}
	rankingPresentationSystem_.Update(deltaSeconds);
	const RankingPresentationFrame& frame = rankingPresentationSystem_.GetFrame();
	if (rankingTitleObject_) {
		rankingTitleObject_->SetScale({
			0.60f * frame.headingScaleFactor,
			0.60f * frame.headingScaleFactor,
			0.60f * frame.headingScaleFactor,
		});
		rankingTitleObject_->Update(titleCamera_.get(), deltaSeconds);
	}

	const GameFlowState& state = GameFlowState::GetInstance();
	const auto currentRank = state.GetLastSubmittedRank();
	const bool hasRankedCurrentRun = currentRank &&
		*currentRank < state.GetRankingCount() &&
		*currentRank < GameFlowState::kRankingCapacity;
	for (std::size_t index = 0;
		index < GameFlowState::kRankingCapacity;
		++index) {
		const bool isCurrentRun = hasRankedCurrentRun && index == *currentRank;
		const Vector4 rowColor = isCurrentRun
			? frame.highlightedRowColor
			: (index == 0 ? kAccentColor : kTextColor);
		lines_[index + 1].SetScale(kRankingTextScale);
		lines_[index + 1].SetColor(index == 0 ? kAccentColor : kTextColor);
		lines_[index + 1].Update();
		rankingValueTexts_[index].SetScale(kRankingTextScale *
			(isCurrentRun ? frame.highlightedRowScale : 1.0f));
		rankingValueTexts_[index].SetColor(rowColor);
		rankingValueTexts_[index].Update();

		if (rankingScoreObjects_[index]) {
			const float scoreScale = kRankingScoreModelScale;
			rankingScoreObjects_[index]->SetScale(
				{ scoreScale, scoreScale, scoreScale });
			rankingScoreObjects_[index]->SetColor(
				index == 0 ? kAccentColor : kTextColor);
			rankingScoreObjects_[index]->Update(titleCamera_.get(), deltaSeconds);
		}
	}
	rankingStatusVisible_ = rankingStatusReady_ && !hasRankedCurrentRun &&
		state.GetLastSubmittedScore().has_value();
	if (rankingStatusVisible_) {
		rankingStatusText_.SetScale(0.80f * frame.markerScale);
		rankingStatusText_.SetColor(frame.highlightedRowColor);
		rankingStatusText_.Update();
	}

	rankingCurrentArrowVisible_ = hasRankedCurrentRun &&
		rankingCurrentArrowSprite_;
	if (rankingCurrentArrowVisible_) {
		const float pulseScale = frame.highlightedRowScale;
		const float rowCenterY = kRankingRowTop +
			kRankingRowStep * static_cast<float>(*currentRank) +
			16.0f * kRankingTextScale;
		rankingCurrentArrowSprite_->SetPosition(
			{ kRankingArrowLeftX,
			  rowCenterY - 0.5f * kRankingArrowHeight * pulseScale });
		rankingCurrentArrowSprite_->SetSize(
			{ kRankingArrowWidth * pulseScale,
			  kRankingArrowHeight * pulseScale });
		rankingCurrentArrowSprite_->SetRotation(0.0f);
		rankingCurrentArrowSprite_->SetColor(frame.highlightedRowColor);
		rankingCurrentArrowSprite_->Update();
	}

	if (rankingPlayerObject_) {
		rankingPlayerObject_->SetPosition(frame.playerPosition);
		rankingPlayerObject_->SetScale({
			frame.playerScale, frame.playerScale, frame.playerScale });
		rankingPlayerObject_->Update(titleCamera_.get(), deltaSeconds);
	}
	for (std::size_t index = 0; index < rankingBallObjects_.size(); ++index) {
		if (!rankingBallObjects_[index]) {
			continue;
		}
		const RankingDecorationTransform& ball = frame.balls[index];
		rankingBallObjects_[index]->SetPosition(ball.position);
		rankingBallObjects_[index]->SetScale(
			{ ball.scale, ball.scale, ball.scale });
		rankingBallObjects_[index]->SetColor(ball.color);
		rankingBallObjects_[index]->Update(titleCamera_.get(), deltaSeconds);
	}
}

void TitleScene::RefreshTransitionPrompt(bool useGamepad)
{
	transitionPromptInitialized_ = true;
	transitionPromptUsesGamepad_ = useGamepad;
	Model* requestedModel = page_ == Page::StageSelect
		? nullptr
		: useGamepad
		? gamepadTransitionModel_
		: keyboardTransitionModel_;
	transitionPromptModelVisible_ = false;
	if (transitionKeyObject_ && requestedModel) {
		const bool modelReady = transitionKeyObject_->GetModel() == requestedModel ||
			transitionKeyObject_->TrySwapStaticModel(requestedModel);
		if (modelReady) {
			if (useGamepad) {
				// The supplied Blender export uses YZ as its text plane and has an
				// off-centre origin. Rotate and offset it to match the Space prompt.
				transitionKeyObject_->SetScale({ 0.80f, 0.80f, 0.80f });
				transitionKeyObject_->SetPosition({ -1.394f, -2.301f, -0.094f });
				transitionKeyObject_->SetRotation({ 0.0f, -kHalfPi, 0.0f });
			} else {
				transitionKeyObject_->SetScale({ 0.60f, 0.60f, 0.60f });
				transitionKeyObject_->SetPosition({ 0.147f, -2.17f, 0.0f });
				transitionKeyObject_->SetRotation({ 0.0f, kPi, 0.0f });
			}
			transitionPromptModelVisible_ = true;
			if (titleCamera_) {
				transitionKeyObject_->Update(titleCamera_.get(), 0.0f);
			}
		}
	}

	std::size_t lineIndex = 0;
	Vector2 position{ 500.0f, 420.0f };
	float scale = 1.05f;
	const char* fallbackText = useGamepad ? "PRESS B" : "PRESS SPACE";
	if (page_ == Page::Instructions) {
		lineIndex = 6;
		position = { useGamepad ? 455.0f : 410.0f, 565.0f };
		scale = 0.95f;
		fallbackText = useGamepad ? "PRESS B TO START" : "PRESS SPACE TO START";
	} else if (page_ == Page::StageSelect) {
		lineIndex = 8;
		position = { 390.0f, 615.0f };
		scale = 0.75f;
		fallbackText = useGamepad
			? "DPAD SELECT   B START"
			: "W/S SELECT   SPACE START";
	} else if (page_ == Page::Ranking) {
		lineIndex = 6;
		position = { useGamepad ? 420.0f : 375.0f, 565.0f };
		scale = 0.95f;
		fallbackText = useGamepad
			? "PRESS B TO CONTINUE"
			: "PRESS SPACE TO CONTINUE";
	}
	SetLine(lineIndex, transitionPromptModelVisible_ ? "" : fallbackText,
		position, scale, kAccentColor);
}

void TitleScene::RefreshStageSelectLines()
{
	constexpr const char* labels[] = {
		"TUTORIAL", "STAGE 01", "STAGE 02", "STAGE 03" };
	SetLine(0, "STAGE SELECT", { 430.0f, 82.0f }, 1.55f, kPrimaryColor);
	for (int index = 0; index < 4; ++index) {
		const bool selected = index == stageSelection_;
		SetLine(
			static_cast<std::size_t>(index + 1),
			std::string(selected ? ">>  " : "    ") + labels[index],
			{ 435.0f, 205.0f + 82.0f * static_cast<float>(index) },
			selected ? 1.08f : 0.88f,
			selected ? kAccentColor : kTextColor);
	}
	SetLine(5, "", {}, 1.0f, kTextColor);
	SetLine(6, "", {}, 1.0f, kTextColor);
	SetLine(7, "", {}, 1.0f, kTextColor);
	SetLine(9, "", {}, 1.0f, kTextColor);
}

void TitleScene::UpdateStageSelectVisuals(float deltaTime)
{
	stageSelectAnimationSeconds_ += (std::max)(0.0f, deltaTime);
	stageSelectParticleTimer_ += (std::max)(0.0f, deltaTime);
	while (stageSelectParticleTimer_ >= 0.075f) {
		stageSelectParticleTimer_ -= 0.075f;
		EmitStageSelectParticles(false);
	}
}

void TitleScene::EmitStageSelectParticles(bool selectionBurst)
{
	Framework* framework = Framework::GetInstance();
	ParticleManager* particles = framework ? framework->GetParticleManager() : nullptr;
	if (!particles) { return; }
	const int count = selectionBurst ? 18 : 2;
	const float selectedY = 1.65f - 0.78f * static_cast<float>(stageSelection_);
	for (int index = 0; index < count; ++index) {
		const float phase = static_cast<float>(stageSelectParticleSequence_++) * 1.6180339f;
		const float side = (stageSelectParticleSequence_ & 1u) ? -1.0f : 1.0f;
		Particle& particle = particles->AddParticle("StageSelectGlow",
			{ side * (2.5f + 0.35f * std::sin(phase)),
				selectionBurst ? selectedY + 0.35f * std::sin(phase * 2.1f)
					: -2.6f + 5.2f * std::fmod(phase * 0.173f, 1.0f), 0.15f });
		particle.velocity = selectionBurst
			? Vector3{ -side * (0.07f + 0.05f * std::fabs(std::sin(phase))),
				0.035f * std::cos(phase), 0.0f }
			: Vector3{ -side * 0.012f, 0.018f + 0.012f * std::sin(phase), 0.0f };
		particle.color = selectionBurst
			? Vector4{ 1.0f, 0.72f, 0.18f, 0.95f }
			: Vector4{ 0.22f, 0.88f, 1.0f, 0.58f };
		particle.lifeTime = selectionBurst ? 0.42f : 1.35f;
		particle.startSize = selectionBurst ? 0.13f : 0.055f;
		particle.endSize = selectionBurst ? 0.015f : 0.018f;
	}
}

void TitleScene::SetLine(std::size_t index, const std::string& text,
	const Vector2& position, float scale, const Vector4& color)
{
	if (index >= lines_.size()) { return; }
	lines_[index].SetText(text);
	lines_[index].SetPosition(position);
	lines_[index].SetScale(scale);
	lines_[index].SetColor(color);
	lines_[index].Update();
	lineCount_ = (std::max)(lineCount_, index + 1);
}
