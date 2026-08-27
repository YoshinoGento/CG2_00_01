#pragma once

#include "farm/core/FarmTypes.h"

#include <cstdint>
#include <string>

enum class FarmFeedbackKind {
	None,
	Harvest,
	Sale,
	EmptySale,
	InputLocked,
	Restarted,
	SeedPurchased,
	CropSelected,
	NoSeed,
	InsufficientMoney,
};

struct FarmFeedbackStats {
	std::uint32_t harvestCount = 0;
	std::uint32_t saleCount = 0;
	std::uint32_t emptySaleCount = 0;
	std::uint32_t goalReachedCount = 0;
	std::uint32_t inputLockedCount = 0;
	std::uint32_t seedPurchaseCount = 0;
	std::uint32_t noSeedCount = 0;
	std::uint32_t insufficientMoneyCount = 0;
};

class FarmFeedbackSystem final {
public:
	void Initialize(bool resetStats = true) noexcept;
	void Update(float deltaTime) noexcept;
	void ShowHarvest(
		farm::CropType crop, int quantity, int qualityScore, int saleValue);
	void ShowSale(int soldCount, int earnedMoney);
	void ShowSale(farm::CropType crop, int soldCount, int earnedMoney);
	void ShowEmptySale(farm::CropType crop = farm::CropType::None);
	void RecordGoalReached() noexcept;
	void ShowClearLocked();
	void ShowRestarted();
	void ShowSeedPurchased(farm::CropType crop, int quantity, int spentMoney);
	void ShowCropSelected(farm::CropType crop);
	void ShowNoSeed(farm::CropType crop);
	void ShowInsufficientMoney();
	void Clear() noexcept;

	[[nodiscard]] const std::string& GetCurrentMessage() const noexcept { return message_; }
	[[nodiscard]] FarmFeedbackKind GetLastKind() const noexcept { return lastKind_; }
	[[nodiscard]] farm::CropType GetLastCrop() const noexcept { return lastCrop_; }
	[[nodiscard]] int GetLastQualityScore() const noexcept { return lastQualityScore_; }
	[[nodiscard]] int GetLastSaleCount() const noexcept { return lastSaleCount_; }
	[[nodiscard]] int GetLastSaleValue() const noexcept { return lastSaleValue_; }
	[[nodiscard]] const FarmFeedbackStats& GetStats() const noexcept { return stats_; }

private:
	void Show(std::string message, FarmFeedbackKind kind);
	static void IncrementSaturated(std::uint32_t& counter) noexcept;

	std::string message_;
	float remainingSeconds_ = 0.0f;
	FarmFeedbackKind lastKind_ = FarmFeedbackKind::None;
	farm::CropType lastCrop_ = farm::CropType::None;
	int lastQualityScore_ = 0;
	int lastSaleCount_ = 0;
	int lastSaleValue_ = 0;
	FarmFeedbackStats stats_{};
};
