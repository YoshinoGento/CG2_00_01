#include "TitleScene.h"

#include "GameFlowState.h"
#include "SceneManager.h"
#include "2d/SpriteCommon.h"
#include "3d/Camera.h"
#include "3d/Model.h"
#include "3d/ModelManager.h"
#include "3d/Object3d.h"
#include "3d/Object3dCommon.h"
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
	if (!spriteCommon ||
		!font_.InitializeFromJson(spriteCommon, "Resources/ui/font/ascii_bitmap_font.json")) {
		return;
	}

	background_ = std::make_unique<Sprite>();
	if (!background_->Initialize(spriteCommon, "Resources/human/white.png")) {
		background_.reset();
		return;
	}
	background_->SetPosition({ 0.0f, 0.0f });
	background_->SetSize({ 1280.0f, 720.0f });
	background_->SetColor({ 0.018f, 0.035f, 0.065f, 1.0f });
	background_->Update();
	for (SpriteText& line : lines_) {
		line.Initialize(spriteCommon, &font_);
		line.SetCharacterSpacing(kCharacterSpacing);
	}

	if (page_ == Page::Title) {
		ModelManager* modelManager = framework->GetModelManager();
		if (modelManager) {
			constexpr const char* kTitleModelPath = "title/Title.obj";
			modelManager->LoadModel(kTitleModelPath);
			Model* titleModel = modelManager->GetModel(kTitleModelPath);
			if (titleModel) {
				titleModel->LoadTextures();
				titleCamera_ = std::make_unique<Camera>();
				titleCamera_->SetTranslate({ 0.0f, 0.0f, -10.0f });
				titleCamera_->SetRotate({ 0.0f, 0.0f, 0.0f });
				titleCamera_->Update();
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
		SetLine(0, "PRESS B OR ENTER", { 445.0f, 420.0f }, 1.05f, kAccentColor);
	} else if (page_ == Page::Instructions) {
		SetLine(0, "HOW TO PLAY", { 465.0f, 105.0f }, 1.55f, kPrimaryColor);
		SetLine(1, "LEFT STICK OR WASD  MOVE", { 350.0f, 215.0f }, 0.95f, kTextColor);
		SetLine(2, "LB RB  ROTATE", { 440.0f, 275.0f }, 0.95f, kTextColor);
		SetLine(3, "RT OR Q  SHOOT MAGNETS", { 360.0f, 335.0f }, 0.95f, kTextColor);
		SetLine(4, "PUT MAGNETS IN GOALS", { 375.0f, 395.0f }, 0.95f, kTextColor);
		SetLine(5, "MENU OR ESC  PAUSE", { 395.0f, 455.0f }, 0.95f, kTextColor);
		SetLine(6, "PRESS B OR ENTER TO START", { 350.0f, 565.0f }, 0.95f, kAccentColor);
	} else {
		SetLine(0, "RANKING", { 520.0f, 75.0f }, 1.65f, kPrimaryColor);
		const auto& state = GameFlowState::GetInstance();
		const auto& ranking = state.GetRanking();
		for (std::size_t index = 0; index < GameFlowState::kRankingCapacity; ++index) {
			char buffer[64]{};
			if (index < state.GetRankingCount()) {
				std::snprintf(buffer, sizeof(buffer), "%zu  SCORE %zu", index + 1, ranking[index]);
			} else {
				std::snprintf(buffer, sizeof(buffer), "%zu  ---", index + 1);
			}
			SetLine(index + 1, buffer,
				{ 490.0f, 190.0f + 60.0f * static_cast<float>(index) },
				1.0f, index == 0 ? kAccentColor : kTextColor);
		}
		SetLine(6, "PRESS B OR ENTER TO CONTINUE", { 315.0f, 565.0f }, 0.95f, kAccentColor);
	}
	uiReady_ = true;
}

void TitleScene::Finalize()
{
	titleObject_.reset();
	titleCamera_.reset();
	background_.reset();
	uiReady_ = false;
}

void TitleScene::Update()
{
	if (titleObject_ && titleCamera_) {
		titleObject_->Update(titleCamera_.get(), 0.0f);
	}
	Input* input = Framework::GetInstance()->GetInput();
	if (!input || (!input->TriggerKey(InputKey::Enter) &&
		!input->TriggerGamepadButton(InputGamepadButton::B))) { return; }
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
	Framework::GetInstance()->GetSpriteCommon()->PreDraw();
	background_->Draw();
	if (titleObject_) {
		Object3dCommon* objectCommon = Framework::GetInstance()->GetObject3dCommon();
		objectCommon->BeginObjectPass();
		titleObject_->Draw();
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
