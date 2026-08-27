#pragma once

#include "farm/core/FarmTypes.h"
#include "farm/data/FarmRules.h"

#include <array>

struct FarmCropQualityResult {
	farm::CropType crop = farm::CropType::None;
	float maturity = 0.0f;
	float waterBalance = 0.0f;
	float terrainFit = 0.0f;
	int score = 0;
	int basePrice = 0;
	int salePrice = 0;

	[[nodiscard]] bool IsValid() const noexcept {
		return farm::IsPlantableCrop(crop) && basePrice > 0 && salePrice > 0;
	}
};

class FarmCropQualitySystem final {
public:
	void Initialize(const farm::FarmRules& rules = {}) noexcept;
	[[nodiscard]] FarmCropQualityResult Evaluate(
		const farm::FarmTile& tile) const noexcept;

private:
	std::array<int, farm::kFarmCropTypeCount> basePrices_{ 120, 170 };
	std::array<float, farm::kFarmCropTypeCount> idealMoisture_{ 0.55f, 0.65f };
	std::array<int, farm::kFarmCropTypeCount> idealHeight_{ 0, 1 };
	float maturityWeight_ = 0.25f;
	float waterBalanceWeight_ = 0.50f;
	float terrainFitWeight_ = 0.25f;
	float heightTolerance_ = 2.0f;
	float minimumPriceMultiplier_ = 0.50f;
	float maximumPriceMultiplier_ = 1.50f;
};
