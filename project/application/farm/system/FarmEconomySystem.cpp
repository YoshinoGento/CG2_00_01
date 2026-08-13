#include "farm/system/FarmEconomySystem.h"

#include <limits>

void FarmEconomySystem::Initialize(const farm::FarmRules& rules)
{
	money_ = rules.initialMoney >= 0 ? rules.initialMoney : 0;
	testCropCount_ = 0;
	testCropSellPrice_ = rules.normalHarvestPrice > 0 ? rules.normalHarvestPrice : 0;
}

bool FarmEconomySystem::AddHarvest(farm::CropType crop, int quantity) noexcept
{
	if (crop != farm::CropType::TestCrop || quantity <= 0 ||
		testCropCount_ > (std::numeric_limits<int>::max)() - quantity) {
		return false;
	}

	testCropCount_ += quantity;
	return true;
}

bool FarmEconomySystem::RemoveHarvest(farm::CropType crop, int quantity) noexcept
{
	if (crop != farm::CropType::TestCrop || quantity <= 0 || quantity > testCropCount_) {
		return false;
	}

	testCropCount_ -= quantity;
	return true;
}

FarmSaleResult FarmEconomySystem::SellAll() noexcept
{
	FarmSaleResult result{};
	if (testCropCount_ <= 0 || testCropSellPrice_ <= 0) {
		return result;
	}

	const long long earnedMoney =
		static_cast<long long>(testCropCount_) * static_cast<long long>(testCropSellPrice_);
	if (earnedMoney > (std::numeric_limits<int>::max)() - static_cast<long long>(money_)) {
		return result;
	}

	result.soldCount = testCropCount_;
	result.earnedMoney = static_cast<int>(earnedMoney);
	money_ += result.earnedMoney;
	testCropCount_ = 0;
	return result;
}

int FarmEconomySystem::GetCropCount(farm::CropType crop) const noexcept
{
	return crop == farm::CropType::TestCrop ? testCropCount_ : 0;
}
