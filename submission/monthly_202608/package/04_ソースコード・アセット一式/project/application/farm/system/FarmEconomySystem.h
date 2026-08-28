#pragma once

#include "farm/core/FarmTypes.h"
#include "farm/data/FarmRules.h"
#include "farm/system/FarmCropQualitySystem.h"

#include <array>

struct FarmSaleResult {
	farm::CropType crop = farm::CropType::None;
	int soldCount = 0;
	int earnedMoney = 0;

	[[nodiscard]] bool Succeeded() const noexcept {
		return soldCount > 0 && earnedMoney > 0;
	}
};

enum class FarmSeedPurchaseStatus {
	None,
	Purchased,
	UnsupportedCrop,
	InvalidQuantity,
	Unavailable,
	InsufficientMoney,
	Overflow,
};

struct FarmSeedPurchaseResult {
	FarmSeedPurchaseStatus status = FarmSeedPurchaseStatus::None;
	farm::CropType crop = farm::CropType::None;
	int purchasedCount = 0;
	int spentMoney = 0;

	[[nodiscard]] bool Succeeded() const noexcept {
		return status == FarmSeedPurchaseStatus::Purchased &&
			purchasedCount > 0 && spentMoney > 0;
	}
};

class FarmEconomySystem final {
public:
	struct Snapshot {
		int money = 0;
		std::array<int, farm::kFarmCropTypeCount> cropCounts{};
		std::array<int, farm::kFarmCropTypeCount> cropValues{};
		std::array<int, farm::kFarmCropTypeCount> sellPrices{};
		std::array<int, farm::kFarmCropTypeCount> seedCounts{};
		std::array<int, farm::kFarmCropTypeCount> seedPrices{};
		FarmCropQualityResult lastHarvestQuality{};
	};

	void Initialize(const farm::FarmRules& rules = {});

	[[nodiscard]] bool AddHarvest(farm::CropType crop, int quantity = 1) noexcept;
	[[nodiscard]] bool RemoveHarvest(farm::CropType crop, int quantity = 1) noexcept;
	[[nodiscard]] bool AddHarvest(
		const FarmCropQualityResult& quality, int quantity = 1) noexcept;
	[[nodiscard]] bool RemoveHarvest(
		const FarmCropQualityResult& quality, int quantity = 1) noexcept;
	[[nodiscard]] bool RemoveHarvest(
		const FarmCropQualityResult& quality, int quantity,
		const FarmCropQualityResult& restoredLastHarvestQuality) noexcept;
	[[nodiscard]] bool AddSeed(farm::CropType crop, int quantity = 1) noexcept;
	[[nodiscard]] bool RemoveSeed(farm::CropType crop, int quantity = 1) noexcept;
	[[nodiscard]] FarmSeedPurchaseResult BuySeed(
		farm::CropType crop, int quantity = 1) noexcept;
	[[nodiscard]] FarmSaleResult SellCrop(farm::CropType crop) noexcept;
	[[nodiscard]] FarmSaleResult SellAll() noexcept;

	[[nodiscard]] int GetMoney() const noexcept { return money_; }
	[[nodiscard]] int GetCropCount(farm::CropType crop) const noexcept;
	[[nodiscard]] int GetTotalCropCount() const noexcept;
	[[nodiscard]] int GetSellPrice(farm::CropType crop) const noexcept;
	[[nodiscard]] int GetCropInventoryValue(farm::CropType crop) const noexcept;
	[[nodiscard]] int GetTestCropSellPrice() const noexcept {
		return GetSellPrice(farm::CropType::TestCrop);
	}
	[[nodiscard]] int GetSeedCount(farm::CropType crop) const noexcept;
	[[nodiscard]] int GetTotalSeedCount() const noexcept;
	[[nodiscard]] int GetSeedPrice(farm::CropType crop) const noexcept;
	[[nodiscard]] int GetSalePreviewValue() const noexcept;
	[[nodiscard]] const FarmCropQualityResult& GetLastHarvestQuality() const noexcept {
		return lastHarvestQuality_;
	}
	[[nodiscard]] Snapshot CaptureSnapshot() const noexcept;
	bool RestoreSnapshot(const Snapshot& snapshot) noexcept;

private:
	int money_ = 0;
	std::array<int, farm::kFarmCropTypeCount> cropCounts_{};
	std::array<int, farm::kFarmCropTypeCount> cropValues_{};
	std::array<int, farm::kFarmCropTypeCount> sellPrices_{};
	std::array<int, farm::kFarmCropTypeCount> seedCounts_{};
	std::array<int, farm::kFarmCropTypeCount> seedPrices_{};
	FarmCropQualityResult lastHarvestQuality_{};
};
