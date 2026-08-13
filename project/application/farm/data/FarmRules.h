#pragma once

#include "math/Matrix.h"

namespace farm {

struct FarmRules {
	int gridWidth = 3;
	int gridHeight = 3;
	float tileSpacing = 2.15f;
	Vector3 origin = { 0.0f, -1.92f, 8.0f };

	float initialCropGrowth = 0.05f;
	float maxCropGrowth = 1.0f;
	float growthPerSecondDry = 0.22f;
	float growthPerSecondWet = 0.48f;
	float wetGrowthMoistureThreshold = 0.05f;
	float moistureDecayPerSecond = 0.08f;
	float maxMoisture = 1.0f;
	float postHarvestMoisture = 0.35f;
	float readyMoistureMinimum = 0.20f;
	float maxUpdateDeltaTime = 0.25f;

	int initialMoney = 300;
	int normalHarvestPrice = 120;
	int rareHarvestPrice = 500;
	int rareHarvestInterval = 3;
};

} // namespace farm
