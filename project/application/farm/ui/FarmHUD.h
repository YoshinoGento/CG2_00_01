#pragma once

#include "2d/BitmapFont.h"
#include "2d/SpriteText.h"

#include <string>

class SpriteCommon;

struct FarmHUDViewData {
	int day = 1;
	int money = 0;
	int rank = 1;
	std::string currentToolName = "None";
	std::string selectedTileInfo;
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
	void UpdateToolText();

private:
	FarmHUDViewData viewData_;

	BitmapFont font_;
	SpriteText dayText_;
	SpriteText moneyText_;
	SpriteText rankText_;
	SpriteText toolText_;
};
