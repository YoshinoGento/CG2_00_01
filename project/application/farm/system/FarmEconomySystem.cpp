#include "farm/system/FarmEconomySystem.h"

#include <algorithm>
#include <limits>

namespace {
int SaturatingTotal(const std::array<int, farm::kFarmCropTypeCount>& values) noexcept
{
	long long total = 0;
	for (const int value : values) {
		total += value;
	}
	return static_cast<int>((std::min)(
		total, static_cast<long long>((std::numeric_limits<int>::max)())));
}

bool AreValuesValid(const std::array<int, farm::kFarmCropTypeCount>& values) noexcept
{
	return std::all_of(values.begin(), values.end(), [](int value) { return value >= 0; });
}

bool TryMultiplyNonNegative(int left, int right, int& result) noexcept
{
	if (left < 0 || right < 0) {
		return false;
	}
	const long long product = static_cast<long long>(left) * right;
	if (product > (std::numeric_limits<int>::max)()) {
		return false;
	}
	result = static_cast<int>(product);
	return true;
}
}

void FarmEconomySystem::Initialize(const farm::FarmRules& rules)
{
	money_ = rules.initialMoney >= 0 ? rules.initialMoney : 0;
	cropCounts_.fill(0);
	cropValues_.fill(0);
	sellPrices_ = {
		rules.normalHarvestPrice > 0 ? rules.normalHarvestPrice : 0,
		rules.carrotHarvestPrice > 0 ? rules.carrotHarvestPrice : 0,
	};
	seedCounts_ = {
		rules.initialTestCropSeedCount >= 0 ? rules.initialTestCropSeedCount : 0,
		rules.initialCarrotSeedCount >= 0 ? rules.initialCarrotSeedCount : 0,
	};
	seedPrices_ = {
		rules.testCropSeedPrice > 0 ? rules.testCropSeedPrice : 0,
		rules.carrotSeedPrice > 0 ? rules.carrotSeedPrice : 0,
	};
	lastHarvestQuality_ = {};
}

bool FarmEconomySystem::AddHarvest(farm::CropType crop, int quantity) noexcept
{
	const int slot = farm::ToCropSlot(crop);
	int addedValue = 0;
	if (slot < 0 || quantity <= 0 ||
		!TryMultiplyNonNegative(sellPrices_[slot], quantity, addedValue) ||
		cropCounts_[slot] > (std::numeric_limits<int>::max)() - quantity ||
		cropValues_[slot] > (std::numeric_limits<int>::max)() - addedValue) {
		return false;
	}
	cropCounts_[slot] += quantity;
	cropValues_[slot] += addedValue;
	return true;
}

bool FarmEconomySystem::RemoveHarvest(farm::CropType crop, int quantity) noexcept
{
	const int slot = farm::ToCropSlot(crop);
	int removedValue = 0;
	if (slot < 0 || quantity <= 0 || quantity > cropCounts_[slot] ||
		!TryMultiplyNonNegative(sellPrices_[slot], quantity, removedValue) ||
		removedValue > cropValues_[slot]) {
		return false;
	}
	cropCounts_[slot] -= quantity;
	cropValues_[slot] -= removedValue;
	return true;
}

bool FarmEconomySystem::AddHarvest(
	const FarmCropQualityResult& quality, int quantity) noexcept
{
	const int slot = farm::ToCropSlot(quality.crop);
	int addedValue = 0;
	if (!quality.IsValid() || slot < 0 || quantity <= 0 ||
		!TryMultiplyNonNegative(quality.salePrice, quantity, addedValue) ||
		cropCounts_[slot] > (std::numeric_limits<int>::max)() - quantity ||
		cropValues_[slot] > (std::numeric_limits<int>::max)() - addedValue) {
		return false;
	}
	cropCounts_[slot] += quantity;
	cropValues_[slot] += addedValue;
	lastHarvestQuality_ = quality;
	return true;
}

bool FarmEconomySystem::RemoveHarvest(
	const FarmCropQualityResult& quality, int quantity) noexcept
{
	return RemoveHarvest(quality, quantity, {});
}

bool FarmEconomySystem::RemoveHarvest(
	const FarmCropQualityResult& quality, int quantity,
	const FarmCropQualityResult& restoredLastHarvestQuality) noexcept
{
	const int slot = farm::ToCropSlot(quality.crop);
	int removedValue = 0;
	if (!quality.IsValid() || slot < 0 || quantity <= 0 ||
		quantity > cropCounts_[slot] ||
		!TryMultiplyNonNegative(quality.salePrice, quantity, removedValue) ||
		removedValue > cropValues_[slot] ||
		(restoredLastHarvestQuality.crop != farm::CropType::None &&
			!restoredLastHarvestQuality.IsValid())) {
		return false;
	}
	cropCounts_[slot] -= quantity;
	cropValues_[slot] -= removedValue;
	lastHarvestQuality_ = restoredLastHarvestQuality;
	return true;
}

bool FarmEconomySystem::AddSeed(farm::CropType crop, int quantity) noexcept
{
	const int slot = farm::ToCropSlot(crop);
	if (slot < 0 || quantity <= 0 ||
		seedCounts_[slot] > (std::numeric_limits<int>::max)() - quantity) {
		return false;
	}
	seedCounts_[slot] += quantity;
	return true;
}

bool FarmEconomySystem::RemoveSeed(farm::CropType crop, int quantity) noexcept
{
	const int slot = farm::ToCropSlot(crop);
	if (slot < 0 || quantity <= 0 || quantity > seedCounts_[slot]) {
		return false;
	}
	seedCounts_[slot] -= quantity;
	return true;
}

FarmSeedPurchaseResult FarmEconomySystem::BuySeed(
	farm::CropType crop, int quantity) noexcept
{
	FarmSeedPurchaseResult result{};
	result.crop = crop;
	const int slot = farm::ToCropSlot(crop);
	if (slot < 0) {
		result.status = FarmSeedPurchaseStatus::UnsupportedCrop;
		return result;
	}
	if (quantity <= 0) {
		result.status = FarmSeedPurchaseStatus::InvalidQuantity;
		return result;
	}
	if (seedPrices_[slot] <= 0) {
		result.status = FarmSeedPurchaseStatus::Unavailable;
		return result;
	}
	if (seedCounts_[slot] > (std::numeric_limits<int>::max)() - quantity) {
		result.status = FarmSeedPurchaseStatus::Overflow;
		return result;
	}

	const long long cost =
		static_cast<long long>(seedPrices_[slot]) * static_cast<long long>(quantity);
	if (cost > (std::numeric_limits<int>::max)()) {
		result.status = FarmSeedPurchaseStatus::Overflow;
		return result;
	}
	if (cost > money_) {
		result.status = FarmSeedPurchaseStatus::InsufficientMoney;
		return result;
	}

	money_ -= static_cast<int>(cost);
	seedCounts_[slot] += quantity;
	result.status = FarmSeedPurchaseStatus::Purchased;
	result.purchasedCount = quantity;
	result.spentMoney = static_cast<int>(cost);
	return result;
}

FarmSaleResult FarmEconomySystem::SellCrop(farm::CropType crop) noexcept
{
	FarmSaleResult result{};
	result.crop = crop;
	const int slot = farm::ToCropSlot(crop);
	if (slot < 0 || cropCounts_[slot] <= 0 || cropValues_[slot] <= 0 ||
		cropValues_[slot] > (std::numeric_limits<int>::max)() - money_) {
		return result;
	}

	result.soldCount = cropCounts_[slot];
	result.earnedMoney = cropValues_[slot];
	money_ += result.earnedMoney;
	cropCounts_[slot] = 0;
	cropValues_[slot] = 0;
	return result;
}

FarmSaleResult FarmEconomySystem::SellAll() noexcept
{
	FarmSaleResult result{};
	long long totalCount = 0;
	long long earnedMoney = 0;
	for (int slot = 0; slot < farm::kFarmCropTypeCount; ++slot) {
		totalCount += cropCounts_[slot];
		earnedMoney += cropValues_[slot];
	}
	if (totalCount <= 0 || earnedMoney <= 0 ||
		totalCount > (std::numeric_limits<int>::max)() ||
		earnedMoney > (std::numeric_limits<int>::max)() - static_cast<long long>(money_)) {
		return result;
	}

	result.soldCount = static_cast<int>(totalCount);
	result.earnedMoney = static_cast<int>(earnedMoney);
	money_ += result.earnedMoney;
	cropCounts_.fill(0);
	cropValues_.fill(0);
	return result;
}

int FarmEconomySystem::GetCropCount(farm::CropType crop) const noexcept
{
	const int slot = farm::ToCropSlot(crop);
	return slot >= 0 ? cropCounts_[slot] : 0;
}

int FarmEconomySystem::GetTotalCropCount() const noexcept
{
	return SaturatingTotal(cropCounts_);
}

int FarmEconomySystem::GetSellPrice(farm::CropType crop) const noexcept
{
	const int slot = farm::ToCropSlot(crop);
	return slot >= 0 ? sellPrices_[slot] : 0;
}

int FarmEconomySystem::GetCropInventoryValue(farm::CropType crop) const noexcept
{
	const int slot = farm::ToCropSlot(crop);
	return slot >= 0 ? cropValues_[slot] : 0;
}

int FarmEconomySystem::GetSeedCount(farm::CropType crop) const noexcept
{
	const int slot = farm::ToCropSlot(crop);
	return slot >= 0 ? seedCounts_[slot] : 0;
}

int FarmEconomySystem::GetTotalSeedCount() const noexcept
{
	return SaturatingTotal(seedCounts_);
}

int FarmEconomySystem::GetSeedPrice(farm::CropType crop) const noexcept
{
	const int slot = farm::ToCropSlot(crop);
	return slot >= 0 ? seedPrices_[slot] : 0;
}

int FarmEconomySystem::GetSalePreviewValue() const noexcept
{
	long long earnedMoney = 0;
	for (int slot = 0; slot < farm::kFarmCropTypeCount; ++slot) {
		earnedMoney += cropValues_[slot];
	}
	if (earnedMoney <= 0 ||
		earnedMoney > (std::numeric_limits<int>::max)() - static_cast<long long>(money_)) {
		return 0;
	}
	return static_cast<int>(earnedMoney);
}

FarmEconomySystem::Snapshot FarmEconomySystem::CaptureSnapshot() const noexcept
{
	return {
		money_, cropCounts_, cropValues_, sellPrices_, seedCounts_, seedPrices_,
		lastHarvestQuality_
	};
}

bool FarmEconomySystem::RestoreSnapshot(const Snapshot& snapshot) noexcept
{
	if (snapshot.money < 0 || !AreValuesValid(snapshot.cropCounts) ||
		!AreValuesValid(snapshot.cropValues) ||
		!AreValuesValid(snapshot.sellPrices) || !AreValuesValid(snapshot.seedCounts) ||
		!AreValuesValid(snapshot.seedPrices) ||
		(snapshot.lastHarvestQuality.crop != farm::CropType::None &&
			!snapshot.lastHarvestQuality.IsValid())) {
		return false;
	}
	money_ = snapshot.money;
	cropCounts_ = snapshot.cropCounts;
	cropValues_ = snapshot.cropValues;
	sellPrices_ = snapshot.sellPrices;
	seedCounts_ = snapshot.seedCounts;
	seedPrices_ = snapshot.seedPrices;
	lastHarvestQuality_ = snapshot.lastHarvestQuality;
	return true;
}
