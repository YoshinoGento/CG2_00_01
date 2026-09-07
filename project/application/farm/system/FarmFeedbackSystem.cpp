#include "farm/system/FarmFeedbackSystem.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace {
constexpr float kFeedbackDurationSeconds = 2.0f;
constexpr float kSaleFeedbackDurationSeconds = 4.0f;
}

void FarmFeedbackSystem::Initialize(bool resetStats) noexcept
{
	lastKind_ = FarmFeedbackKind::None;
	lastCrop_ = farm::CropType::None;
	lastQualityScore_ = 0;
	lastSaleCount_ = 0;
	lastSaleValue_ = 0;
	if (resetStats) {
		stats_ = {};
	}
	Clear();
}

void FarmFeedbackSystem::Update(float deltaTime) noexcept
{
	if (message_.empty() || !std::isfinite(deltaTime) || deltaTime <= 0.0f) {
		return;
	}
	remainingSeconds_ = (std::max)(0.0f, remainingSeconds_ - deltaTime);
	if (remainingSeconds_ <= 0.0f) {
		Clear();
	}
}

void FarmFeedbackSystem::ShowHarvest(
	farm::CropType crop, int quantity, int qualityScore, int saleValue)
{
	if (farm::IsPlantableCrop(crop) && quantity > 0 && saleValue > 0) {
		IncrementSaturated(stats_.harvestCount);
		lastCrop_ = crop;
		Show(
			"HARVEST +" + std::to_string(quantity) + " Q" +
				std::to_string(std::clamp(qualityScore, 0, 100)) + " " +
				std::to_string((std::max)(saleValue, 0)) + "G",
			FarmFeedbackKind::Harvest);
		lastQualityScore_ = std::clamp(qualityScore, 0, 100);
		lastSaleValue_ = saleValue;
	}
}

void FarmFeedbackSystem::ShowSale(int soldCount, int earnedMoney)
{
	ShowSale(farm::CropType::None, soldCount, earnedMoney);
}

void FarmFeedbackSystem::ShowSale(
	farm::CropType crop, int soldCount, int earnedMoney)
{
	if (soldCount > 0 && earnedMoney > 0) {
		IncrementSaturated(stats_.saleCount);
		lastCrop_ = farm::IsPlantableCrop(crop) ? crop : farm::CropType::None;
		Show("SOLD " + std::to_string(soldCount) + " CROP  +" +
			std::to_string(earnedMoney) + "G", FarmFeedbackKind::Sale);
		lastSaleCount_ = soldCount;
		lastSaleValue_ = earnedMoney;
	}
}

void FarmFeedbackSystem::ShowEmptySale(farm::CropType crop)
{
	IncrementSaturated(stats_.emptySaleCount);
	lastCrop_ = farm::IsPlantableCrop(crop) ? crop : farm::CropType::None;
	Show("NO CROPS TO SELL", FarmFeedbackKind::EmptySale);
}

void FarmFeedbackSystem::RecordGoalReached() noexcept
{
	IncrementSaturated(stats_.goalReachedCount);
}

void FarmFeedbackSystem::ShowClearLocked()
{
	IncrementSaturated(stats_.inputLockedCount);
	Show("FARM LOCKED - F5 TO RETRY", FarmFeedbackKind::InputLocked);
}

void FarmFeedbackSystem::ShowRestarted()
{
	Show("FARM RESTARTED", FarmFeedbackKind::Restarted);
}

void FarmFeedbackSystem::ShowSeedPurchased(
	farm::CropType crop, int quantity, int spentMoney)
{
	if (!farm::IsPlantableCrop(crop) || quantity <= 0 || spentMoney <= 0) {
		return;
	}
	IncrementSaturated(stats_.seedPurchaseCount);
	lastCrop_ = crop;
	Show(
		"BOUGHT " + std::to_string(quantity) + " " + farm::ToString(crop) + " SEED  -" +
		std::to_string(spentMoney) + "G",
		FarmFeedbackKind::SeedPurchased);
}

void FarmFeedbackSystem::ShowCropSelected(farm::CropType crop)
{
	if (!farm::IsPlantableCrop(crop)) {
		return;
	}
	lastCrop_ = crop;
	Show(
		std::string("SELECTED ") + farm::ToString(crop),
		FarmFeedbackKind::CropSelected);
}

void FarmFeedbackSystem::ShowNoSeed(farm::CropType crop)
{
	IncrementSaturated(stats_.noSeedCount);
	lastCrop_ = farm::IsPlantableCrop(crop) ? crop : farm::CropType::None;
	Show(
		std::string("NO ") + farm::ToString(lastCrop_) + " SEEDS - PRESS B TO BUY",
		FarmFeedbackKind::NoSeed);
}

void FarmFeedbackSystem::ShowInsufficientMoney()
{
	IncrementSaturated(stats_.insufficientMoneyCount);
	Show("NOT ENOUGH MONEY", FarmFeedbackKind::InsufficientMoney);
}

void FarmFeedbackSystem::Clear() noexcept
{
	message_.clear();
	remainingSeconds_ = 0.0f;
}

void FarmFeedbackSystem::Show(std::string message, FarmFeedbackKind kind)
{
	message_ = std::move(message);
	remainingSeconds_ = kind == FarmFeedbackKind::Sale
		? kSaleFeedbackDurationSeconds : kFeedbackDurationSeconds;
	lastKind_ = kind;
	lastQualityScore_ = 0;
	lastSaleCount_ = 0;
	lastSaleValue_ = 0;
}

void FarmFeedbackSystem::IncrementSaturated(std::uint32_t& counter) noexcept
{
	if (counter < (std::numeric_limits<std::uint32_t>::max)()) {
		++counter;
	}
}
