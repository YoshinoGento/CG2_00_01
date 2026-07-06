#include "farm/ui/FarmHUD.h"

#include "2d/SpriteCommon.h"
#include "base/Logger.h"

#include <string>

namespace {
	const char* kFontConfigPath = "Resources/ui/font/ascii_bitmap_font.json";

	const Vector2 kDayTextPosition{ 20.0f, 20.0f };
	const Vector2 kMoneyTextPosition{ 20.0f, 56.0f };
	const Vector2 kRankTextPosition{ 20.0f, 92.0f };
	const Vector2 kToolTextPosition{ 20.0f, 128.0f };

	constexpr float kTextScale = 0.75f;
	constexpr float kCharacterSpacing = -4.0f;
	constexpr Vector4 kTextColor{ 1.0f, 1.0f, 1.0f, 1.0f };
}

bool FarmHUD::Initialize(SpriteCommon* spriteCommon)
{
	if (spriteCommon == nullptr) {
		Logger::Log("FarmHUD::Initialize failed. spriteCommon is null.");
		return false;
	}

	if (!font_.InitializeFromJson(spriteCommon, kFontConfigPath)) {
		Logger::Log("FarmHUD::Initialize failed. font initialization failed.");
		return false;
	}

	dayText_.Initialize(spriteCommon, &font_);
	moneyText_.Initialize(spriteCommon, &font_);
	rankText_.Initialize(spriteCommon, &font_);
	toolText_.Initialize(spriteCommon, &font_);

	ApplyLayout();
	RefreshAllText();
	return true;
}

void FarmHUD::SetViewData(const FarmHUDViewData& viewData)
{
	const bool dayChanged = viewData_.day != viewData.day;
	const bool moneyChanged = viewData_.money != viewData.money;
	const bool rankChanged = viewData_.rank != viewData.rank;
	const bool toolChanged = viewData_.currentToolName != viewData.currentToolName;

	viewData_ = viewData;

	if (dayChanged) {
		UpdateDayText();
	}
	if (moneyChanged) {
		UpdateMoneyText();
	}
	if (rankChanged) {
		UpdateRankText();
	}
	if (toolChanged) {
		UpdateToolText();
	}
}

void FarmHUD::Update(float deltaTime)
{
	(void)deltaTime;

	dayText_.Update();
	moneyText_.Update();
	rankText_.Update();
	toolText_.Update();
}

void FarmHUD::Draw()
{
	dayText_.Draw();
	moneyText_.Draw();
	rankText_.Draw();
	toolText_.Draw();
}

void FarmHUD::ApplyLayout()
{
	dayText_.SetPosition(kDayTextPosition);
	moneyText_.SetPosition(kMoneyTextPosition);
	rankText_.SetPosition(kRankTextPosition);
	toolText_.SetPosition(kToolTextPosition);

	dayText_.SetScale(kTextScale);
	moneyText_.SetScale(kTextScale);
	rankText_.SetScale(kTextScale);
	toolText_.SetScale(kTextScale);

	dayText_.SetCharacterSpacing(kCharacterSpacing);
	moneyText_.SetCharacterSpacing(kCharacterSpacing);
	rankText_.SetCharacterSpacing(kCharacterSpacing);
	toolText_.SetCharacterSpacing(kCharacterSpacing);

	dayText_.SetColor(kTextColor);
	moneyText_.SetColor(kTextColor);
	rankText_.SetColor(kTextColor);
	toolText_.SetColor(kTextColor);
}

void FarmHUD::RefreshAllText()
{
	UpdateDayText();
	UpdateMoneyText();
	UpdateRankText();
	UpdateToolText();
}

void FarmHUD::UpdateDayText()
{
	dayText_.SetText("Day " + std::to_string(viewData_.day));
}

void FarmHUD::UpdateMoneyText()
{
	moneyText_.SetText("Money " + std::to_string(viewData_.money) + "G");
}

void FarmHUD::UpdateRankText()
{
	rankText_.SetText("Rank " + std::to_string(viewData_.rank));
}

void FarmHUD::UpdateToolText()
{
	toolText_.SetText("Tool " + viewData_.currentToolName);
}
