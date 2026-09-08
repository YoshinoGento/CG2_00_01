#include "TitleScene.h"

#include "GameFlowState.h"
#include "SceneManager.h"
#include "2d/SpriteCommon.h"
#include "3d/Camera.h"
#include "3d/Model.h"
#include "3d/ModelManager.h"
#include "3d/Object3d.h"
#include "3d/Object3dCommon.h"
#include "3d/Skybox.h"
#include "base/Framework.h"
#include "io/Input.h"

#include <algorithm>
#include <cstdio>

namespace {
constexpr Vector4 kPrimaryColor{ 0.32f, 0.95f, 1.0f, 1.0f };
constexpr Vector4 kTextColor{ 0.88f, 0.94f, 0.98f, 1.0f };
constexpr Vector4 kAccentColor{ 1.0f, 0.82f, 0.24f, 1.0f };
constexpr float kCharacterSpacing = -7.0f;
}

void TitleScene::Initialize()
{
	Framework* framework = Framework::GetInstance();
	SpriteCommon* spriteCommon = framework ? framework->GetSpriteCommon() : nullptr;
	GameFlowState::GetInstance().EnsureBgm(framework ? framework->GetAudio() : nullptr);
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
		constexpr const char* kSpaceModelPath = "title/Space.obj";
		modelManager->LoadModel(kSpaceModelPath);
		Model* spaceModel = modelManager->GetModel(kSpaceModelPath);
		if (spaceModel) {
			spaceModel->LoadTextures();
			transitionKeyObject_ = std::make_unique<Object3d>();
			transitionKeyObject_->Initialize(framework->GetObject3dCommon());
			transitionKeyObject_->SetModel(spaceModel);
			transitionKeyObject_->SetScale({ 0.60f, 0.60f, 0.60f });
			transitionKeyObject_->SetPosition({ 0.147f, -2.17f, 0.0f });
			transitionKeyObject_->SetRotation({ 0.0f, 3.14159265f, 0.0f });
			transitionKeyObject_->SetColor(kAccentColor);
			transitionKeyObject_->SetEnableLighting(false);
			transitionKeyObject_->SetCullMode(0);
			transitionKeyObject_->Update(titleCamera_.get(), 0.0f);
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
		if (!transitionKeyObject_) {
			SetLine(0, "PRESS SPACE", { 500.0f, 420.0f }, 1.05f, kAccentColor);
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
		if (!transitionKeyObject_) {
			SetLine(6, "PRESS SPACE TO START", { 410.0f, 565.0f }, 0.95f, kAccentColor);
		}
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
		if (!transitionKeyObject_) {
			SetLine(6, "PRESS SPACE TO CONTINUE", { 375.0f, 565.0f }, 0.95f, kAccentColor);
		}
	}
	uiReady_ = true;
}

void TitleScene::Finalize()
{
	for (auto& scoreObject : rankingScoreObjects_) { scoreObject.reset(); }
	rankingTitleObject_.reset();
	operationGuideObject_.reset();
	guideTitleObject_.reset();
	transitionKeyObject_.reset();
	titleObject_.reset();
	menuSkybox_.reset();
	titleCamera_.reset();
	uiReady_ = false;
}

void TitleScene::Update()
{
	if (titleObject_ && titleCamera_) {
		titleObject_->Update(titleCamera_.get(), 0.0f);
	}
	if (transitionKeyObject_ && titleCamera_) {
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
	if (!input || !input->TriggerKey(InputKey::Space)) { return; }
	if (page_ == Page::Title) {
		SceneManager::GetInstance()->ChangeScene("INSTRUCTIONS");
	} else if (page_ == Page::Instructions) {
		SceneManager::GetInstance()->ChangeScene("MAGNET_PROTOTYPE");
	} else {
		SceneManager::GetInstance()->ChangeScene("INSTRUCTIONS");
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
		if (transitionKeyObject_) { transitionKeyObject_->Draw(); }
		if (guideTitleObject_) { guideTitleObject_->Draw(); }
		if (operationGuideObject_) { operationGuideObject_->Draw(); }
		if (rankingTitleObject_) { rankingTitleObject_->Draw(); }
		for (auto& scoreObject : rankingScoreObjects_) {
			if (scoreObject) { scoreObject->Draw(); }
		}
		objectCommon->EndObjectPass();
	}
	Framework::GetInstance()->GetSpriteCommon()->PreDraw();
	for (std::size_t index = 0; index < lineCount_; ++index) {
		lines_[index].Draw();
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
