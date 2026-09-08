#include "TitleScene.h"

#include "GameFlowState.h"
#include "SceneManager.h"
#include "2d/SpriteCommon.h"
#include "2d/TextureManager.h"
#include "3d/Camera.h"
#include "3d/Model.h"
#include "3d/ModelManager.h"
#include "3d/Object3d.h"
#include "3d/Object3dCommon.h"
#include "3d/Skybox.h"
#include "base/FrameClock.h"
#include "base/Framework.h"
#include "io/Input.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {
constexpr Vector4 kPrimaryColor{ 0.32f, 0.95f, 1.0f, 1.0f };
constexpr Vector4 kTextColor{ 0.88f, 0.94f, 0.98f, 1.0f };
constexpr Vector4 kAccentColor{ 1.0f, 0.82f, 0.24f, 1.0f };
constexpr float kCharacterSpacing = -7.0f;
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
				rankingTitleObject_->SetScale({ 0.53f, 0.53f, 0.53f });
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
					scoreObject->SetScale({ 0.18f, 0.18f, 0.18f });
					scoreObject->SetPosition(
						{ -0.08f, 0.94f - 0.385f * static_cast<float>(index), 0.0f });
					scoreObject->SetRotation({ 0.0f, 3.14159265f, 0.0f });
					scoreObject->SetColor(index == 0 ? kAccentColor : kTextColor);
					scoreObject->SetEnableLighting(false);
					scoreObject->SetCullMode(0);
					scoreObject->Update(titleCamera_.get(), 0.0f);
				}
			}
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
		InitializeStageSelectVisuals(spriteCommon);
		RefreshStageSelectLines();
	} else {
		if (!rankingTitleObject_) {
			SetLine(0, "RANKING", { 520.0f, 75.0f }, 1.65f, kPrimaryColor);
		}
		const auto& state = GameFlowState::GetInstance();
		const auto& ranking = state.GetRanking();
		for (std::size_t index = 0; index < GameFlowState::kRankingCapacity; ++index) {
			char buffer[64]{};
			if (index < state.GetRankingCount()) {
				std::snprintf(buffer, sizeof(buffer), "%zu        %zu", index + 1, ranking[index]);
			} else {
				std::snprintf(buffer, sizeof(buffer), "%zu  ---", index + 1);
			}
			SetLine(index + 1, buffer,
				{ 490.0f, 190.0f + 60.0f * static_cast<float>(index) },
				1.0f, index == 0 ? kAccentColor : kTextColor);
		}
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
	for (auto& accent : stageSelectAccentBars_) { accent.reset(); }
	for (auto& card : stageSelectCards_) { card.reset(); }
	stageSelectFooterLine_.reset();
	stageSelectHeaderLine_.reset();
	stageSelectBackdrop_.reset();
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
	uiReady_ = false;
}

void TitleScene::Update()
{
	if (page_ == Page::StageSelect) {
		Framework* framework = Framework::GetInstance();
		const FrameClock* clock = framework ? framework->GetFrameClock() : nullptr;
		UpdateStageSelectVisuals(
			clock ? clock->GetFrameDeltaSeconds() : FrameClock::kDefaultFixedDeltaSeconds);
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
	if (rankingTitleObject_ && titleCamera_) {
		rankingTitleObject_->Update(titleCamera_.get(), 0.0f);
	}
	for (auto& scoreObject : rankingScoreObjects_) {
		if (scoreObject && titleCamera_) {
			scoreObject->Update(titleCamera_.get(), 0.0f);
		}
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
		}
		if (moveDown) {
			stageSelection_ = (stageSelection_ + 1) % 4;
			RefreshStageSelectLines();
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
		operationGuideObject_ || rankingTitleObject_ || rankingScoreObjects_[0]) {
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
		objectCommon->EndObjectPass();
	}
	Framework::GetInstance()->GetSpriteCommon()->PreDraw();
	if (stageSelectBackdrop_) { stageSelectBackdrop_->Draw(); }
	if (stageSelectHeaderLine_) { stageSelectHeaderLine_->Draw(); }
	if (stageSelectFooterLine_) { stageSelectFooterLine_->Draw(); }
	for (auto& card : stageSelectCards_) {
		if (card) { card->Draw(); }
	}
	for (auto& accent : stageSelectAccentBars_) {
		if (accent) { accent->Draw(); }
	}
	for (std::size_t index = 0; index < lineCount_; ++index) {
		lines_[index].Draw();
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
	constexpr const char* descriptions[] = {
		"LEARN MAGNET CONTROL",
		"STANDARD MAGNET FIELD",
		"GIMMICK TRAINING ZONE",
		"MAXIMUM HAZARD CIRCUIT" };
	constexpr const char* difficulties[] = {
		"DIFFICULTY  *", "DIFFICULTY  **", "DIFFICULTY  ***", "DIFFICULTY  *****" };
	SetLine(0, "STAGE SELECT", { 430.0f, 62.0f }, 1.55f, kPrimaryColor);
	SetLine(9, "MAGNETIC FIELD // SELECT MISSION", { 423.0f, 116.0f }, 0.55f,
		{ 0.72f, 0.86f, 0.94f, 1.0f });
	for (int index = 0; index < 4; ++index) {
		const bool selected = index == stageSelection_;
		SetLine(
			static_cast<std::size_t>(index + 1),
			std::string(selected ? ">>  " : "    ") + labels[index],
			{ 435.0f, 174.0f + 76.0f * static_cast<float>(index) },
			selected ? 1.08f : 0.88f,
			selected ? kAccentColor : kTextColor);
	}
	SetLine(5, descriptions[stageSelection_], { 415.0f, 505.0f }, 0.74f, kPrimaryColor);
	SetLine(6, difficulties[stageSelection_], { 485.0f, 545.0f }, 0.68f, kAccentColor);
	SetLine(7, "CHARGE  CONNECT  LAUNCH", { 455.0f, 580.0f }, 0.52f,
		{ 0.74f, 0.80f, 0.90f, 1.0f });
}

void TitleScene::InitializeStageSelectVisuals(SpriteCommon* spriteCommon)
{
	if (!spriteCommon) { return; }
	const auto makePanel = [spriteCommon](const char* texturePath,
		const Vector2& position, const Vector2& size,
		const Vector4& color) -> std::unique_ptr<Sprite> {
		auto sprite = std::make_unique<Sprite>();
		if (!sprite->Initialize(spriteCommon, texturePath)) { return nullptr; }
		sprite->SetPosition(position);
		sprite->SetSize(size);
		sprite->SetColor(color);
		sprite->Update();
		return sprite;
	};

	stageSelectBackdrop_ = makePanel(
		"Resources/ui/stage_select/stage_select_frame.png",
		{ 90.0f, 48.0f }, { 1100.0f, 620.0f }, { 1.0f, 1.0f, 1.0f, 0.98f });
	stageSelectHeaderLine_ = makePanel("Resources/human/white.png",
		{ 335.0f, 40.0f }, { 610.0f, 4.0f }, kPrimaryColor);
	stageSelectFooterLine_ = makePanel("Resources/human/white.png",
		{ 335.0f, 653.0f }, { 610.0f, 4.0f },
		{ 1.0f, 0.25f, 0.62f, 1.0f });
	for (int index = 0; index < 4; ++index) {
		const float y = 147.0f + 76.0f * static_cast<float>(index);
		stageSelectCards_[index] = makePanel(
			"Resources/ui/stage_select/stage_card.png",
			{ 365.0f, y }, { 550.0f, 72.0f },
			{ 0.68f, 0.78f, 0.84f, 0.90f });
		stageSelectAccentBars_[index] = makePanel("Resources/human/white.png",
			{ 365.0f, y + 7.0f }, { 7.0f, 58.0f },
			{ 0.32f, 0.95f, 1.0f, 0.45f });
	}
}

void TitleScene::UpdateStageSelectVisuals(float deltaTime)
{
	stageSelectAnimationSeconds_ += (std::max)(0.0f, deltaTime);
	const float pulse = 0.5f + 0.5f * std::sin(stageSelectAnimationSeconds_ * 4.5f);
	for (int index = 0; index < 4; ++index) {
		const bool selected = index == stageSelection_;
		if (stageSelectCards_[index]) {
			stageSelectCards_[index]->SetPosition({ selected ? 350.0f : 365.0f,
				147.0f + 76.0f * static_cast<float>(index) });
			stageSelectCards_[index]->SetSize({ selected ? 580.0f : 550.0f, 72.0f });
			stageSelectCards_[index]->SetColor(selected
				? Vector4{ 0.94f + pulse * 0.06f, 0.92f + pulse * 0.08f, 1.0f, 1.0f }
				: Vector4{ 0.58f, 0.68f, 0.76f, 0.86f });
			stageSelectCards_[index]->Update();
		}
		if (stageSelectAccentBars_[index]) {
			stageSelectAccentBars_[index]->SetPosition({ selected ? 350.0f : 365.0f,
				154.0f + 76.0f * static_cast<float>(index) });
			stageSelectAccentBars_[index]->SetSize({ selected ? 11.0f : 5.0f, 58.0f });
			stageSelectAccentBars_[index]->SetColor(selected
				? Vector4{ 1.0f, 0.68f + pulse * 0.22f, 0.18f, 1.0f }
				: Vector4{ 0.32f, 0.95f, 1.0f, 0.35f });
			stageSelectAccentBars_[index]->Update();
		}
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
