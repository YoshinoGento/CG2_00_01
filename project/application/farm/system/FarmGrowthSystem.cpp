#include "farm/system/FarmGrowthSystem.h"

#include "farm/core/FarmGrid.h"
#include "farm/system/FarmSoilSystem.h"

#include <algorithm>
#include <cmath>
#include <limits>

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

bool CareHistoriesEqual(
	const farm::FarmCropCareHistory& left,
	const farm::FarmCropCareHistory& right) noexcept
{
	return left.drySeconds == right.drySeconds &&
		left.lowSeconds == right.lowSeconds &&
		left.goodSeconds == right.goodSeconds &&
		left.excessSeconds == right.excessSeconds &&
		left.efficiencySeconds == right.efficiencySeconds &&
		left.nutrientGrowth == right.nutrientGrowth && left.nutrientSupply == right.nutrientSupply;
}

float SaturatingAdd(float value, float increment) noexcept
{
	const double sum = static_cast<double>(value) + static_cast<double>(increment);
	return sum < static_cast<double>((std::numeric_limits<float>::max)())
		? static_cast<float>(sum)
		: (std::numeric_limits<float>::max)();
}

void AccumulateCareHistory(
	farm::FarmCropCareHistory& history,
	FarmMoistureStatus status,
	float seconds,
	float efficiency) noexcept
{
	if (!std::isfinite(seconds) || seconds <= 0.0f ||
		!std::isfinite(efficiency)) {
		return;
	}
	float* duration = nullptr;
	switch (status) {
	case FarmMoistureStatus::Dry: duration = &history.drySeconds; break;
	case FarmMoistureStatus::Low: duration = &history.lowSeconds; break;
	case FarmMoistureStatus::Good: duration = &history.goodSeconds; break;
	case FarmMoistureStatus::Excess: duration = &history.excessSeconds; break;
	case FarmMoistureStatus::Invalid:
	default: return;
	}
	*duration = SaturatingAdd(*duration, seconds);
	history.efficiencySeconds = SaturatingAdd(
		history.efficiencySeconds,
		seconds * std::clamp(efficiency, 0.0f, 1.0f));
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
	result.goodMoistureMaximum = SanitizeNormalized(
		profile.goodMoistureMaximum, fallback.goodMoistureMaximum);
	if (result.dryMoistureThreshold >= result.goodMoistureMinimum ||
		result.goodMoistureMinimum >= result.goodMoistureMaximum ||
		result.goodMoistureMaximum >= 1.0f) {
		result.dryMoistureThreshold = fallback.dryMoistureThreshold;
		result.goodMoistureMinimum = fallback.goodMoistureMinimum;
		result.goodMoistureMaximum = fallback.goodMoistureMaximum;
	}
	result.saturatedGrowthMultiplier = SanitizeNormalized(
		profile.saturatedGrowthMultiplier, fallback.saturatedGrowthMultiplier);
	return result;
}

struct MoistureResponse {
	FarmMoistureStatus status;
	float rate;
};

// Thresholds are sanitized at Initialize. Runtime and forecasts share this curve.
MoistureResponse EvaluateMoisture(const farm::FarmCropGrowthProfile& profile, float moisture) noexcept
{
	if (moisture <= profile.dryMoistureThreshold) {
		return {FarmMoistureStatus::Dry, profile.growthPerSecondDry};
	}
	if (moisture < profile.goodMoistureMinimum) {
		const float t = (moisture - profile.dryMoistureThreshold) /
			(profile.goodMoistureMinimum - profile.dryMoistureThreshold);
		return {FarmMoistureStatus::Low, std::lerp(profile.growthPerSecondDry, profile.growthPerSecondWet, t)};
	}
	if (moisture <= profile.goodMoistureMaximum) {
		return {FarmMoistureStatus::Good, profile.growthPerSecondWet};
	}
	const float t = (moisture - profile.goodMoistureMaximum) / (1.0f - profile.goodMoistureMaximum);
	return {FarmMoistureStatus::Excess,
		profile.growthPerSecondWet * std::lerp(1.0f, profile.saturatedGrowthMultiplier, t)};
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

bool FarmGrowthSystem::Update(
	farm::FarmGrid& grid,
	float deltaTime,
	float timeScale) const
{
	if (!std::isfinite(deltaTime) || !std::isfinite(timeScale) ||
		deltaTime <= 0.0f || timeScale <= 0.0f) {
		return false;
	}

	const float scaledDeltaTime =
		(std::min)(deltaTime, rules_.maxUpdateDeltaTime) * timeScale;
	if (!std::isfinite(scaledDeltaTime) || scaledDeltaTime <= 0.0f) {
		return false;
	}
	bool changed = false;
	for (int tileIndex = 0; tileIndex < grid.GetTileCount(); ++tileIndex) {
		farm::FarmTile* tile = grid.GetMutableTile(tileIndex);
		if (tile == nullptr) {
			continue;
		}

		const float previousMoisture = tile->moisture;
		const float previousGrowth = tile->growth;
		const farm::FarmCropCareHistory previousCareHistory = tile->careHistory;
		tile->moisture = SanitizeTileValue(tile->moisture);
		tile->growth = SanitizeTileValue(tile->growth);
		if (!tile->careHistory.IsValid() ||
			(tile->state != farm::FarmTileState::Planted &&
			 !tile->careHistory.IsEmpty())) {
			tile->careHistory = {};
		}
		changed |= previousMoisture != tile->moisture ||
			previousGrowth != tile->growth ||
			!CareHistoriesEqual(previousCareHistory, tile->careHistory);
		if (!IsCultivated(*tile) || tile->state != farm::FarmTileState::Planted ||
			tile->crop == farm::CropType::None || farm::IsHarvestReady(*tile)) {
			continue;
		}
		const farm::FarmCropGrowthProfile* profile =
			GetGrowthProfile(rules_, tile->crop);
		if (profile == nullptr) {
			continue;
		}

		const MoistureResponse response = EvaluateMoisture(*profile, tile->moisture);
		const float growthPerSecond = response.rate;
		const float efficiency = profile->growthPerSecondWet > 0.0f
			? growthPerSecond / profile->growthPerSecondWet
			: 0.0f;
		AccumulateCareHistory(
			tile->careHistory, response.status, scaledDeltaTime, efficiency);
		tile->growth = std::clamp(
			tile->growth + growthPerSecond * scaledDeltaTime,
			kNormalizedMinimum,
			kNormalizedMaximum);
		FarmSoilSystem::Grow(*tile, tile->growth - previousGrowth);
		tile->moisture = std::clamp(
			tile->moisture - profile->moistureDecayPerSecond * scaledDeltaTime,
			kNormalizedMinimum,
			kNormalizedMaximum);
		changed |= previousMoisture != tile->moisture ||
			previousGrowth != tile->growth ||
			!CareHistoriesEqual(previousCareHistory, tile->careHistory);
	}
	return changed;
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
	if (!IsCultivated(tile) ||
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
	result.goodMoistureMaximum = profile->goodMoistureMaximum;
	const auto response = EvaluateMoisture(*profile, moisture);
	result.moistureStatus = response.status;
	result.growthEfficiency = profile->growthPerSecondWet > 0.0f
		? response.rate / profile->growthPerSecondWet : 0.0f;

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

	result.growthPerSecond = response.rate * timeScale;
	if (result.growthPerSecond > 0.0f && std::isfinite(result.growthPerSecond)) {
		result.secondsUntilReady =
			(kNormalizedMaximum - growth) / result.growthPerSecond;
	}
	return result;
}
