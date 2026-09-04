#include "farm/system/FarmGrowthSystem.h"

#include "farm/core/FarmGrid.h"
#include "farm/system/FarmIrrigationSystem.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr float kNormalizedMinimum = 0.0f;
constexpr float kNormalizedMaximum = 1.0f;

float SanitizeNonNegative(float value, float fallback) noexcept
{
	return std::isfinite(value) && value >= 0.0f ? value : fallback;
}

float SanitizePositive(float value, float fallback) noexcept
{
	return std::isfinite(value) && value > 0.0f ? value : fallback;
}

float SanitizeNormalized(float value, float fallback) noexcept
{
	return std::isfinite(value)
		? std::clamp(value, kNormalizedMinimum, kNormalizedMaximum)
		: fallback;
}

float SanitizeTileValue(float value) noexcept
{
	return std::isfinite(value)
		? std::clamp(value, kNormalizedMinimum, kNormalizedMaximum)
		: kNormalizedMinimum;
}

bool IsCultivated(const farm::FarmTile& tile) noexcept
{
	return tile.feature == farm::FarmTileFeature::None &&
		tile.state != farm::FarmTileState::Empty;
}

farm::FarmCropGrowthProfile SanitizeProfile(
	const farm::FarmCropGrowthProfile& profile,
	const farm::FarmCropGrowthProfile& fallback) noexcept
{
	farm::FarmCropGrowthProfile result;
	result.growthPerSecondDry = SanitizeNonNegative(
		profile.growthPerSecondDry, fallback.growthPerSecondDry);
	result.growthPerSecondWet = SanitizeNonNegative(
		profile.growthPerSecondWet, fallback.growthPerSecondWet);
	result.growthPerSecondWet = (std::max)(
		result.growthPerSecondWet, result.growthPerSecondDry);
	result.dryMoistureThreshold = SanitizeNormalized(
		profile.dryMoistureThreshold, fallback.dryMoistureThreshold);
	result.moistureDecayPerSecond = SanitizeNonNegative(
		profile.moistureDecayPerSecond, fallback.moistureDecayPerSecond);
	result.goodMoistureMinimum = SanitizeNormalized(
		profile.goodMoistureMinimum, fallback.goodMoistureMinimum);
	result.goodMoistureMinimum = (std::max)(
		result.goodMoistureMinimum, result.dryMoistureThreshold);
	return result;
}

const farm::FarmCropGrowthProfile* GetGrowthProfile(
	const farm::FarmRules& rules, farm::CropType crop) noexcept
{
	switch (crop) {
	case farm::CropType::TestCrop:
		return &rules.testCropGrowth;
	case farm::CropType::Carrot:
		return &rules.carrotGrowth;
	case farm::CropType::None:
	default:
		return nullptr;
	}
}
}

void FarmGrowthSystem::Initialize(const farm::FarmRules& rules) noexcept
{
	const farm::FarmRules defaults{};
	rules_ = rules;
	rules_.testCropGrowth = SanitizeProfile(
		rules.testCropGrowth, defaults.testCropGrowth);
	rules_.carrotGrowth = SanitizeProfile(
		rules.carrotGrowth, defaults.carrotGrowth);
	rules_.maxUpdateDeltaTime = SanitizePositive(
		rules.maxUpdateDeltaTime, defaults.maxUpdateDeltaTime);
	rules_.irrigationMoistureRecoveryPerSecond = SanitizeNonNegative(
		rules.irrigationMoistureRecoveryPerSecond,
		defaults.irrigationMoistureRecoveryPerSecond);
}

void FarmGrowthSystem::Update(
	farm::FarmGrid& grid,
	const farm::FarmIrrigationSystem& irrigationSystem,
	float deltaTime,
	float timeScale) const
{
	if (!std::isfinite(deltaTime) || !std::isfinite(timeScale) ||
		deltaTime <= 0.0f || timeScale <= 0.0f) {
		return;
	}

	const float scaledDeltaTime =
		(std::min)(deltaTime, rules_.maxUpdateDeltaTime) * timeScale;
	if (!std::isfinite(scaledDeltaTime) || scaledDeltaTime <= 0.0f) {
		return;
	}
	for (int tileIndex = 0; tileIndex < grid.GetTileCount(); ++tileIndex) {
		farm::FarmTile* tile = grid.GetMutableTile(tileIndex);
		if (tile == nullptr) {
			continue;
		}

		tile->moisture = SanitizeTileValue(tile->moisture);
		tile->growth = SanitizeTileValue(tile->growth);
		const float irrigationStrength = std::clamp(
			irrigationSystem.GetIrrigationStrength(tileIndex),
			kNormalizedMinimum,
			kNormalizedMaximum);
		if (IsCultivated(*tile) && irrigationStrength > 0.0f) {
			tile->moisture = std::clamp(
				tile->moisture +
					rules_.irrigationMoistureRecoveryPerSecond *
						irrigationStrength * scaledDeltaTime,
				kNormalizedMinimum,
				kNormalizedMaximum);
		}
		if (tile->state != farm::FarmTileState::Planted ||
			tile->crop == farm::CropType::None || farm::IsHarvestReady(*tile)) {
			continue;
		}
		const farm::FarmCropGrowthProfile* profile =
			GetGrowthProfile(rules_, tile->crop);
		if (profile == nullptr) {
			continue;
		}

		const float growthPerSecond = profile->growthPerSecondDry +
			(profile->growthPerSecondWet - profile->growthPerSecondDry) * tile->moisture;
		tile->growth = std::clamp(
			tile->growth + growthPerSecond * scaledDeltaTime,
			kNormalizedMinimum,
			kNormalizedMaximum);
		tile->moisture = std::clamp(
			tile->moisture - profile->moistureDecayPerSecond * scaledDeltaTime,
			kNormalizedMinimum,
			kNormalizedMaximum);
	}
}

FarmGrowthForecast FarmGrowthSystem::Evaluate(
	const farm::FarmTile& tile,
	farm::CropType previewCrop,
	float timeScale,
	float irrigationStrength) const noexcept
{
	FarmGrowthForecast result;
	result.irrigationStrength = SanitizeNormalized(irrigationStrength, 0.0f);
	result.irrigationAvailable = result.irrigationStrength > 0.0f;
	if (tile.state == farm::FarmTileState::Empty ||
		!std::isfinite(tile.moisture) || !std::isfinite(tile.growth)) {
		return result;
	}
	result.irrigationActive = result.irrigationAvailable && IsCultivated(tile);
	const farm::CropType profileCrop = farm::IsPlantableCrop(tile.crop)
		? tile.crop : previewCrop;
	const farm::FarmCropGrowthProfile* profile =
		GetGrowthProfile(rules_, profileCrop);
	if (profile == nullptr) {
		return result;
	}

	const float moisture = SanitizeTileValue(tile.moisture);
	const float growth = SanitizeTileValue(tile.growth);
	result.moistureValid = true;
	result.profileCrop = profileCrop;
	result.goodMoistureMinimum = profile->goodMoistureMinimum;
	if (moisture <= profile->dryMoistureThreshold) {
		result.moistureStatus = FarmMoistureStatus::Dry;
	} else if (moisture < profile->goodMoistureMinimum) {
		result.moistureStatus = FarmMoistureStatus::Low;
	} else {
		result.moistureStatus = FarmMoistureStatus::Good;
	}

	if (!std::isfinite(timeScale) || timeScale <= 0.0f) {
		return result;
	}
	result.irrigationRecoveryPerSecond = result.irrigationActive
		? rules_.irrigationMoistureRecoveryPerSecond *
			result.irrigationStrength * timeScale
		: 0.0f;
	result.growing = tile.state == farm::FarmTileState::Planted &&
		tile.crop != farm::CropType::None && growth < kNormalizedMaximum;
	result.moistureDecayPerSecond = result.growing
		? profile->moistureDecayPerSecond * timeScale
		: 0.0f;
	result.netMoisturePerSecond =
		result.irrigationRecoveryPerSecond - result.moistureDecayPerSecond;
	if (result.netMoisturePerSecond < 0.0f &&
		std::isfinite(result.netMoisturePerSecond)) {
		result.secondsUntilDry = moisture / -result.netMoisturePerSecond;
	} else if (result.netMoisturePerSecond > 0.0f &&
		std::isfinite(result.netMoisturePerSecond)) {
		result.secondsUntilFullMoisture =
			(kNormalizedMaximum - moisture) / result.netMoisturePerSecond;
	}
	if (!result.growing) {
		return result;
	}

	result.growthPerSecond =
		(profile->growthPerSecondDry +
			(profile->growthPerSecondWet - profile->growthPerSecondDry) * moisture) *
		timeScale;
	if (result.growthPerSecond > 0.0f && std::isfinite(result.growthPerSecond)) {
		result.secondsUntilReady =
			(kNormalizedMaximum - growth) / result.growthPerSecond;
	}
	return result;
}
