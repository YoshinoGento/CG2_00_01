#pragma once

#include "2d/BitmapFont.h"
#include "2d/SpriteText.h"

#include <string>

class SpriteCommon;

struct FarmHUDViewData {
	int day = 1;
	int money = 0;
	int rank = 1;
	float timeScale = 1.0f;
	std::string currentToolName = "None";
	std::string toolGuide;
	std::string selectedTileInfo;
	std::string selectedTileHint;
	std::string temporaryMessage;
};

class FarmHUD {
public:
	bool Initialize(SpriteCommon* spriteCommon);
	void SetViewData(const FarmHUDViewData& viewData);
	void Update(float deltaTime);
	void Draw();

private:
	void ApplyLayout();
	void RefreshAllText();
	void UpdateDayText();
	void UpdateMoneyText();
	void UpdateRankText();
	void UpdateTimeScaleText();
	void UpdateToolText();
	void UpdateToolGuideText();
	void UpdateSelectedTileInfoText();
	void UpdateSelectedTileHintText();

private:
	FarmHUDViewData viewData_;

	BitmapFont font_;
	SpriteText dayShadowText_;
	SpriteText moneyShadowText_;
	SpriteText rankShadowText_;
	SpriteText timeScaleShadowText_;
	SpriteText toolShadowText_;
	SpriteText toolGuideShadowText_;
	SpriteText selectedTileInfoShadowText_;
	SpriteText selectedTileHintShadowText_;
	SpriteText dayText_;
	SpriteText moneyText_;
	SpriteText rankText_;
	SpriteText timeScaleText_;
	SpriteText toolText_;
	SpriteText toolGuideText_;
	SpriteText selectedTileInfoText_;
	SpriteText selectedTileHintText_;
};
