#pragma once

#include "farm/core/FarmTypes.h"
#include "farm/data/FarmRules.h"

namespace farm {
class FarmGrid;
}

enum class FarmMoistureStatus {
	Invalid,
	Dry,
	Low,
	Good,
	Excess,
};

struct FarmGrowthForecast {
	bool moistureValid = false;
	bool growing = false;
	farm::CropType profileCrop = farm::CropType::None;
	FarmMoistureStatus moistureStatus = FarmMoistureStatus::Invalid;
	float goodMoistureMinimum = 0.0f;
	float goodMoistureMaximum = 0.0f;
	float growthEfficiency = 0.0f;
	float growthPerSecond = 0.0f;
	float moistureDecayPerSecond = 0.0f;
	float irrigationRecoveryPerSecond = 0.0f;
	float irrigationStrength = 0.0f;
	float netMoisturePerSecond = 0.0f;
	float secondsUntilDry = -1.0f;
	float secondsUntilFullMoisture = -1.0f;
	float secondsUntilReady = -1.0f;
	bool irrigationAvailable = false;
	bool irrigationActive = false;
};

class FarmGrowthSystem final {
public:
	void Initialize(const farm::FarmRules& rules = {}) noexcept;
	// Reports persistent tile changes, including repaired nonfinite values.
	bool Update(
		farm::FarmGrid& grid,
		float deltaTime,
		float timeScale) const;
	[[nodiscard]] FarmGrowthForecast Evaluate(
		const farm::FarmTile& tile,
		farm::CropType previewCrop = farm::CropType::None,
		float timeScale = 1.0f,
		float irrigationStrength = 0.0f) const noexcept;

private:
	farm::FarmRules rules_{};
};
