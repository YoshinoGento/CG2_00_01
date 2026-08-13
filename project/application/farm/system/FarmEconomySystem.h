#pragma once

#include "farm/core/FarmTypes.h"
#include "farm/data/FarmRules.h"

struct FarmSaleResult {
	int soldCount = 0;
	int earnedMoney = 0;

	[[nodiscard]] bool Succeeded() const noexcept {
		return soldCount > 0 && earnedMoney > 0;
	}
};

class FarmEconomySystem final {
public:
	void Initialize(const farm::FarmRules& rules = {});

	[[nodiscard]] bool AddHarvest(farm::CropType crop, int quantity = 1) noexcept;
	[[nodiscard]] bool RemoveHarvest(farm::CropType crop, int quantity = 1) noexcept;
	[[nodiscard]] FarmSaleResult SellAll() noexcept;

	[[nodiscard]] int GetMoney() const noexcept { return money_; }
	[[nodiscard]] int GetCropCount(farm::CropType crop) const noexcept;
	[[nodiscard]] int GetTotalCropCount() const noexcept { return testCropCount_; }
	[[nodiscard]] int GetTestCropSellPrice() const noexcept { return testCropSellPrice_; }

private:
	int money_ = 0;
	int testCropCount_ = 0;
	int testCropSellPrice_ = 0;
};
