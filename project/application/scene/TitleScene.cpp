#include "TitleScene.h"

#include "GameFlowState.h"
#include "SceneManager.h"
#include "2d/SpriteCommon.h"
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
		SetLine(0, "MAGNET 10 DAYS", { 405.0f, 210.0f }, 1.9f, kPrimaryColor);
		SetLine(1, "PRESS ENTER", { 500.0f, 420.0f }, 1.05f, kAccentColor);
	} else if (page_ == Page::Instructions) {
		SetLine(0, "HOW TO PLAY", { 465.0f, 105.0f }, 1.55f, kPrimaryColor);
		SetLine(1, "WASD  MOVE", { 455.0f, 235.0f }, 1.0f, kTextColor);
		SetLine(2, "Q     SHOOT MAGNETS", { 390.0f, 295.0f }, 1.0f, kTextColor);
		SetLine(3, "PUT MAGNETS IN GOALS", { 375.0f, 355.0f }, 1.0f, kTextColor);
		SetLine(4, "ESC   PAUSE MENU", { 410.0f, 415.0f }, 1.0f, kTextColor);
		SetLine(5, "PRESS ENTER TO START", { 400.0f, 550.0f }, 1.0f, kAccentColor);
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
		SetLine(6, "PRESS ENTER TO CONTINUE", { 365.0f, 565.0f }, 1.0f, kAccentColor);
	}
	uiReady_ = true;
}

void TitleScene::Finalize()
{
	background_.reset();
	uiReady_ = false;
}

void TitleScene::Update()
{
	Input* input = Framework::GetInstance()->GetInput();
	if (!input || !input->TriggerKey(InputKey::Enter)) { return; }
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
