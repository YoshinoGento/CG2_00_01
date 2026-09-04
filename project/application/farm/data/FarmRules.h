#pragma once

#include "math/Matrix.h"

namespace farm {

struct FarmCropGrowthProfile {
	float growthPerSecondDry = 1.0f / 25.0f;
	float growthPerSecondWet = 1.0f / 10.0f;
	float dryMoistureThreshold = 0.05f;
	float moistureDecayPerSecond = 1.0f / 60.0f;
	float goodMoistureMinimum = 0.20f;
};

struct FarmRules {
	int gridWidth = 3;
	int gridHeight = 3;
	float tileSpacing = 2.15f;
	Vector3 origin = { 0.0f, -1.92f, 8.0f };

	float initialCropGrowth = 0.05f;
	float maxCropGrowth = 1.0f;
	FarmCropGrowthProfile testCropGrowth{};
	FarmCropGrowthProfile carrotGrowth{
		1.0f / 36.0f,
		1.0f / 15.0f,
		0.08f,
		1.0f / 55.0f,
		0.35f,
	};
	float maxMoisture = 1.0f;
	float irrigationMoistureRecoveryPerSecond = 1.0f / 8.0f;
	float irrigationSourceStrength = 1.0f;
	float irrigationCanalTransferEfficiency = 0.90f;
	float postHarvestMoisture = 0.35f;
	float maxUpdateDeltaTime = 0.25f;

	int initialMoney = 300;
	int initialTestCropSeedCount = 0;
	int testCropSeedPrice = 40;
	int initialCarrotSeedCount = 0;
	int carrotSeedPrice = 60;
	int carrotHarvestPrice = 170;
	int clearMoneyTarget = 540;
	int normalHarvestPrice = 120;
	int rareHarvestPrice = 500;
	int rareHarvestInterval = 3;

	float testCropIdealHarvestMoisture = 0.55f;
	float carrotIdealHarvestMoisture = 0.65f;
	int testCropIdealHeight = 0;
	int carrotIdealHeight = 1;
	float qualityMaturityWeight = 0.25f;
	float qualityWaterBalanceWeight = 0.50f;
	float qualityTerrainFitWeight = 0.25f;
	float qualityHeightTolerance = 2.0f;
	float minimumQualityPriceMultiplier = 0.50f;
	float maximumQualityPriceMultiplier = 1.50f;
};

} // namespace farm
