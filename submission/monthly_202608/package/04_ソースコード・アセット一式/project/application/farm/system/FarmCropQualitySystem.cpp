#include "farm/system/FarmCropQualitySystem.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
constexpr float kMinimumPositiveValue = 0.0001f;

float SanitizeNormalized(float value, float fallback) noexcept
{
	return std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : fallback;
}

float SanitizePositive(float value, float fallback) noexcept
{
	return std::isfinite(value) && value > kMinimumPositiveValue ? value : fallback;
}
}

void FarmCropQualitySystem::Initialize(const farm::FarmRules& rules) noexcept
{
	basePrices_ = {
		rules.normalHarvestPrice > 0 ? rules.normalHarvestPrice : 120,
		rules.carrotHarvestPrice > 0 ? rules.carrotHarvestPrice : 170,
	};
	idealMoisture_ = {
		SanitizeNormalized(rules.testCropIdealHarvestMoisture, 0.55f),
		SanitizeNormalized(rules.carrotIdealHarvestMoisture, 0.65f),
	};
	idealHeight_ = {
		std::max(rules.testCropIdealHeight, 0),
		std::max(rules.carrotIdealHeight, 0),
	};
	maturityWeight_ = SanitizePositive(rules.qualityMaturityWeight, 0.25f);
	waterBalanceWeight_ = SanitizePositive(rules.qualityWaterBalanceWeight, 0.50f);
	terrainFitWeight_ = SanitizePositive(rules.qualityTerrainFitWeight, 0.25f);
	heightTolerance_ = SanitizePositive(rules.qualityHeightTolerance, 2.0f);
	minimumPriceMultiplier_ = SanitizePositive(
		rules.minimumQualityPriceMultiplier, 0.50f);
	maximumPriceMultiplier_ = SanitizePositive(
		rules.maximumQualityPriceMultiplier, 1.50f);
	if (maximumPriceMultiplier_ < minimumPriceMultiplier_) {
		std::swap(maximumPriceMultiplier_, minimumPriceMultiplier_);
	}
}

FarmCropQualityResult FarmCropQualitySystem::Evaluate(
	const farm::FarmTile& tile) const noexcept
{
	FarmCropQualityResult result{};
	result.crop = tile.crop;
	const int slot = farm::ToCropSlot(tile.crop);
	if (slot < 0 || !std::isfinite(tile.growth) || !std::isfinite(tile.moisture) ||
		basePrices_[slot] <= 0) {
		return result;
	}

	result.maturity = std::clamp(tile.growth, 0.0f, 1.0f);
	const float moisture = std::clamp(tile.moisture, 0.0f, 1.0f);
	const float idealMoisture = idealMoisture_[slot];
	const float moistureRange = (std::max)(
		(std::max)(idealMoisture, 1.0f - idealMoisture), kMinimumPositiveValue);
	result.waterBalance = std::clamp(
		1.0f - std::abs(moisture - idealMoisture) / moistureRange,
		0.0f,
		1.0f);
	result.terrainFit = std::clamp(
		1.0f - std::abs(static_cast<float>(tile.heightLevel - idealHeight_[slot])) /
			heightTolerance_,
		0.0f,
		1.0f);

	const float totalWeight =
		maturityWeight_ + waterBalanceWeight_ + terrainFitWeight_;
	if (!std::isfinite(totalWeight) || totalWeight <= kMinimumPositiveValue) {
		return {};
	}
	const float normalizedScore = std::clamp(
		(result.maturity * maturityWeight_ +
			result.waterBalance * waterBalanceWeight_ +
			result.terrainFit * terrainFitWeight_) /
			totalWeight,
		0.0f,
		1.0f);
	result.score = static_cast<int>(std::lround(normalizedScore * 100.0f));
	result.basePrice = basePrices_[slot];
	const double multiplier = static_cast<double>(minimumPriceMultiplier_) +
		static_cast<double>(normalizedScore) *
			static_cast<double>(maximumPriceMultiplier_ - minimumPriceMultiplier_);
	const double price = static_cast<double>(result.basePrice) * multiplier;
	if (!std::isfinite(price) || price <= 0.0 ||
		price > static_cast<double>((std::numeric_limits<int>::max)())) {
		return {};
	}
	result.salePrice = static_cast<int>(std::lround(price));
	return result;
}
