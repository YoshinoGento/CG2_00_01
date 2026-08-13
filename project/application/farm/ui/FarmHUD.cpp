#include "farm/ui/FarmHUD.h"

#include "2d/SpriteCommon.h"
#include "base/Logger.h"

#include <cmath>
#include <string>

namespace {
	const char* kFontConfigPath = "Resources/ui/font/ascii_bitmap_font.json";

	const Vector2 kDayTextPosition{ 32.0f, 32.0f };
	const Vector2 kMoneyTextPosition{ 32.0f, 88.0f };
	const Vector2 kRankTextPosition{ 32.0f, 144.0f };
	const Vector2 kCropCountTextPosition{ 32.0f, 200.0f };
	const Vector2 kToolTextPosition{ 420.0f, 32.0f };
	const Vector2 kToolGuideTextPosition{ 250.0f, 92.0f };
	const Vector2 kTimeScaleTextPosition{ 1080.0f, 32.0f };
	const Vector2 kSelectedTileInfoTextPosition{ 32.0f, 600.0f };
	const Vector2 kSelectedTileHintTextPosition{ 32.0f, 650.0f };
	const Vector2 kTextShadowOffset{ 2.0f, 2.0f };

	constexpr float kTextScale = 1.35f;
	constexpr float kCharacterSpacing = -8.0f;
	constexpr float kToolTextScale = 1.70f;
	constexpr float kToolGuideTextScale = 0.62f;
	constexpr float kToolGuideCharacterSpacing = -5.0f;
	constexpr float kTimeScaleTextScale = 1.5f;
	constexpr float kTimeScaleCharacterSpacing = -8.0f;
	constexpr float kSelectedTileInfoTextScale = 0.85f;
	constexpr float kSelectedTileInfoCharacterSpacing = -5.0f;
	constexpr float kTimeScaleEpsilon = 0.001f;
	constexpr Vector4 kTextColor{ 1.0f, 1.0f, 1.0f, 1.0f };
	constexpr Vector4 kTextShadowColor{ 0.0f, 0.0f, 0.0f, 0.75f };
	constexpr Vector4 kToolColor{ 1.0f, 0.86f, 0.18f, 1.0f };
	constexpr Vector4 kToolGuideColor{ 0.72f, 0.92f, 1.0f, 1.0f };
	constexpr Vector4 kTileHintColor{ 1.0f, 0.88f, 0.28f, 1.0f };

	Vector2 WithShadowOffset(const Vector2& position)
	{
		return {
			position.x + kTextShadowOffset.x,
			position.y + kTextShadowOffset.y,
		};
	}

	bool IsSameTimeScale(float lhs, float rhs)
	{
		return std::fabs(lhs - rhs) <= kTimeScaleEpsilon;
	}

	std::string FormatTimeScaleText(float timeScale)
	{
		const char* multiplicationSign = "\xC3\x97";
		if (IsSameTimeScale(timeScale, 1.0f)) {
			return std::string(multiplicationSign) + "1";
		}
		if (IsSameTimeScale(timeScale, 2.0f)) {
			return std::string(multiplicationSign) + "2";
		}
		if (IsSameTimeScale(timeScale, 4.0f)) {
			return std::string(multiplicationSign) + "4";
		}
		return std::string(multiplicationSign) + "?";
	}
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

	dayShadowText_.Initialize(spriteCommon, &font_);
	moneyShadowText_.Initialize(spriteCommon, &font_);
	rankShadowText_.Initialize(spriteCommon, &font_);
	cropCountShadowText_.Initialize(spriteCommon, &font_);
	timeScaleShadowText_.Initialize(spriteCommon, &font_);
	toolShadowText_.Initialize(spriteCommon, &font_);
	toolGuideShadowText_.Initialize(spriteCommon, &font_);
	selectedTileInfoShadowText_.Initialize(spriteCommon, &font_);
	selectedTileHintShadowText_.Initialize(spriteCommon, &font_);
	dayText_.Initialize(spriteCommon, &font_);
	moneyText_.Initialize(spriteCommon, &font_);
	rankText_.Initialize(spriteCommon, &font_);
	cropCountText_.Initialize(spriteCommon, &font_);
	timeScaleText_.Initialize(spriteCommon, &font_);
	toolText_.Initialize(spriteCommon, &font_);
	toolGuideText_.Initialize(spriteCommon, &font_);
	selectedTileInfoText_.Initialize(spriteCommon, &font_);
	selectedTileHintText_.Initialize(spriteCommon, &font_);

	ApplyLayout();
	RefreshAllText();
	return true;
}

void FarmHUD::SetViewData(const FarmHUDViewData& viewData)
{
	const bool dayChanged = viewData_.day != viewData.day;
	const bool moneyChanged = viewData_.money != viewData.money;
	const bool rankChanged = viewData_.rank != viewData.rank;
	const bool cropCountChanged = viewData_.cropCount != viewData.cropCount;
	const bool timeScaleChanged = !IsSameTimeScale(viewData_.timeScale, viewData.timeScale);
	const bool toolChanged = viewData_.currentToolName != viewData.currentToolName;
	const bool toolGuideChanged = viewData_.toolGuide != viewData.toolGuide;
	const bool selectedTileInfoChanged = viewData_.selectedTileInfo != viewData.selectedTileInfo;
	const bool selectedTileHintChanged = viewData_.selectedTileHint != viewData.selectedTileHint;

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
	if (cropCountChanged) {
		UpdateCropCountText();
	}
	if (timeScaleChanged) {
		UpdateTimeScaleText();
	}
	if (toolChanged) {
		UpdateToolText();
	}
	if (toolGuideChanged) {
		UpdateToolGuideText();
	}
	if (selectedTileInfoChanged) {
		UpdateSelectedTileInfoText();
	}
	if (selectedTileHintChanged) {
		UpdateSelectedTileHintText();
	}
}

void FarmHUD::Update(float deltaTime)
{
	(void)deltaTime;

	dayText_.Update();
	moneyText_.Update();
	rankText_.Update();
	cropCountText_.Update();
	timeScaleText_.Update();
	toolText_.Update();
	toolGuideText_.Update();
	selectedTileInfoText_.Update();
	selectedTileHintText_.Update();
	dayShadowText_.Update();
	moneyShadowText_.Update();
	rankShadowText_.Update();
	cropCountShadowText_.Update();
	timeScaleShadowText_.Update();
	toolShadowText_.Update();
	toolGuideShadowText_.Update();
	selectedTileInfoShadowText_.Update();
	selectedTileHintShadowText_.Update();
}

void FarmHUD::Draw()
{
	dayShadowText_.Draw();
	dayText_.Draw();
	moneyShadowText_.Draw();
	moneyText_.Draw();
	rankShadowText_.Draw();
	rankText_.Draw();
	cropCountShadowText_.Draw();
	cropCountText_.Draw();
	toolShadowText_.Draw();
	toolText_.Draw();
	toolGuideShadowText_.Draw();
	toolGuideText_.Draw();
	selectedTileInfoShadowText_.Draw();
	selectedTileInfoText_.Draw();
	selectedTileHintShadowText_.Draw();
	selectedTileHintText_.Draw();
	timeScaleShadowText_.Draw();
	timeScaleText_.Draw();
}

void FarmHUD::ApplyLayout()
{
	dayShadowText_.SetPosition(WithShadowOffset(kDayTextPosition));
	moneyShadowText_.SetPosition(WithShadowOffset(kMoneyTextPosition));
	rankShadowText_.SetPosition(WithShadowOffset(kRankTextPosition));
	cropCountShadowText_.SetPosition(WithShadowOffset(kCropCountTextPosition));
	toolShadowText_.SetPosition(WithShadowOffset(kToolTextPosition));
	toolGuideShadowText_.SetPosition(WithShadowOffset(kToolGuideTextPosition));
	timeScaleShadowText_.SetPosition(WithShadowOffset(kTimeScaleTextPosition));
	selectedTileInfoShadowText_.SetPosition(WithShadowOffset(kSelectedTileInfoTextPosition));
	selectedTileHintShadowText_.SetPosition(WithShadowOffset(kSelectedTileHintTextPosition));
	dayText_.SetPosition(kDayTextPosition);
	moneyText_.SetPosition(kMoneyTextPosition);
	rankText_.SetPosition(kRankTextPosition);
	cropCountText_.SetPosition(kCropCountTextPosition);
	toolText_.SetPosition(kToolTextPosition);
	toolGuideText_.SetPosition(kToolGuideTextPosition);
	timeScaleText_.SetPosition(kTimeScaleTextPosition);
	selectedTileInfoText_.SetPosition(kSelectedTileInfoTextPosition);
	selectedTileHintText_.SetPosition(kSelectedTileHintTextPosition);

	dayShadowText_.SetScale(kTextScale);
	moneyShadowText_.SetScale(kTextScale);
	rankShadowText_.SetScale(kTextScale);
	cropCountShadowText_.SetScale(kTextScale);
	toolShadowText_.SetScale(kToolTextScale);
	toolGuideShadowText_.SetScale(kToolGuideTextScale);
	timeScaleShadowText_.SetScale(kTimeScaleTextScale);
	selectedTileInfoShadowText_.SetScale(kSelectedTileInfoTextScale);
	selectedTileHintShadowText_.SetScale(kSelectedTileInfoTextScale);
	dayText_.SetScale(kTextScale);
	moneyText_.SetScale(kTextScale);
	rankText_.SetScale(kTextScale);
	cropCountText_.SetScale(kTextScale);
	toolText_.SetScale(kToolTextScale);
	toolGuideText_.SetScale(kToolGuideTextScale);
	timeScaleText_.SetScale(kTimeScaleTextScale);
	selectedTileInfoText_.SetScale(kSelectedTileInfoTextScale);
	selectedTileHintText_.SetScale(kSelectedTileInfoTextScale);

	dayShadowText_.SetCharacterSpacing(kCharacterSpacing);
	moneyShadowText_.SetCharacterSpacing(kCharacterSpacing);
	rankShadowText_.SetCharacterSpacing(kCharacterSpacing);
	cropCountShadowText_.SetCharacterSpacing(kCharacterSpacing);
	toolShadowText_.SetCharacterSpacing(kCharacterSpacing);
	toolGuideShadowText_.SetCharacterSpacing(kToolGuideCharacterSpacing);
	timeScaleShadowText_.SetCharacterSpacing(kTimeScaleCharacterSpacing);
	selectedTileInfoShadowText_.SetCharacterSpacing(kSelectedTileInfoCharacterSpacing);
	selectedTileHintShadowText_.SetCharacterSpacing(kSelectedTileInfoCharacterSpacing);
	dayText_.SetCharacterSpacing(kCharacterSpacing);
	moneyText_.SetCharacterSpacing(kCharacterSpacing);
	rankText_.SetCharacterSpacing(kCharacterSpacing);
	cropCountText_.SetCharacterSpacing(kCharacterSpacing);
	toolText_.SetCharacterSpacing(kCharacterSpacing);
	toolGuideText_.SetCharacterSpacing(kToolGuideCharacterSpacing);
	timeScaleText_.SetCharacterSpacing(kTimeScaleCharacterSpacing);
	selectedTileInfoText_.SetCharacterSpacing(kSelectedTileInfoCharacterSpacing);
	selectedTileHintText_.SetCharacterSpacing(kSelectedTileInfoCharacterSpacing);

	dayShadowText_.SetColor(kTextShadowColor);
	moneyShadowText_.SetColor(kTextShadowColor);
	rankShadowText_.SetColor(kTextShadowColor);
	cropCountShadowText_.SetColor(kTextShadowColor);
	toolShadowText_.SetColor(kTextShadowColor);
	toolGuideShadowText_.SetColor(kTextShadowColor);
	timeScaleShadowText_.SetColor(kTextShadowColor);
	selectedTileInfoShadowText_.SetColor(kTextShadowColor);
	selectedTileHintShadowText_.SetColor(kTextShadowColor);
	dayText_.SetColor(kTextColor);
	moneyText_.SetColor(kTextColor);
	rankText_.SetColor(kTextColor);
	cropCountText_.SetColor(kTextColor);
	toolText_.SetColor(kToolColor);
	toolGuideText_.SetColor(kToolGuideColor);
	timeScaleText_.SetColor(kTextColor);
	selectedTileInfoText_.SetColor(kTextColor);
	selectedTileHintText_.SetColor(kTileHintColor);
}

void FarmHUD::RefreshAllText()
{
	UpdateDayText();
	UpdateMoneyText();
	UpdateRankText();
	UpdateCropCountText();
	UpdateTimeScaleText();
	UpdateToolText();
	UpdateToolGuideText();
	UpdateSelectedTileInfoText();
	UpdateSelectedTileHintText();
}

void FarmHUD::UpdateDayText()
{
	const std::string text = "Day " + std::to_string(viewData_.day);
	dayShadowText_.SetText(text);
	dayText_.SetText(text);
}

void FarmHUD::UpdateMoneyText()
{
	const std::string text = "Money " + std::to_string(viewData_.money) + "G";
	moneyShadowText_.SetText(text);
	moneyText_.SetText(text);
}

void FarmHUD::UpdateRankText()
{
	const std::string text = "Rank " + std::to_string(viewData_.rank);
	rankShadowText_.SetText(text);
	rankText_.SetText(text);
}

void FarmHUD::UpdateCropCountText()
{
	const std::string text = "Crop " + std::to_string(viewData_.cropCount);
	cropCountShadowText_.SetText(text);
	cropCountText_.SetText(text);
}

void FarmHUD::UpdateTimeScaleText()
{
	const std::string text = FormatTimeScaleText(viewData_.timeScale);
	timeScaleShadowText_.SetText(text);
	timeScaleText_.SetText(text);
}

void FarmHUD::UpdateToolText()
{
	const std::string text = "TOOL " + viewData_.currentToolName;
	toolShadowText_.SetText(text);
	toolText_.SetText(text);
}

void FarmHUD::UpdateToolGuideText()
{
	toolGuideShadowText_.SetText(viewData_.toolGuide);
	toolGuideText_.SetText(viewData_.toolGuide);
}

void FarmHUD::UpdateSelectedTileInfoText()
{
	selectedTileInfoShadowText_.SetText(viewData_.selectedTileInfo);
	selectedTileInfoText_.SetText(viewData_.selectedTileInfo);
}

void FarmHUD::UpdateSelectedTileHintText()
{
	selectedTileHintShadowText_.SetText(viewData_.selectedTileHint);
	selectedTileHintText_.SetText(viewData_.selectedTileHint);
}
