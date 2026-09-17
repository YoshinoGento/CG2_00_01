#include "farm/system/FarmProgressionSystem.h"

#include <algorithm>
#include <limits>

void FarmProgressionSystem::Initialize(const farm::FarmRules& rules, FarmProgressionMode mode) noexcept
{
	targetMoney_ = (std::max)(1, rules.clearMoneyTarget);
	cleared_ = false;
	mode_ = ValidMode(mode) ? mode : FarmProgressionMode::Trial;
}

bool FarmProgressionSystem::EvaluateClear(int currentMoney) noexcept
{
	if (IsContestSeason() || cleared_ || currentMoney < targetMoney_) {
		return false;
	}
	cleared_ = true;
	return true;
}

bool FarmProgressionSystem::EvaluateSeason(const FarmContestSeasonSummary& summary) noexcept {
	if (!IsContestSeason() || cleared_ || !summary.valid || !summary.finalized) return false;
	cleared_ = true;
	return true;
}

bool FarmProgressionSystem::SetMode(FarmProgressionMode mode, int money, const FarmContestSeasonSummary& summary) noexcept {
	if (!ValidMode(mode) || money < 0 || !summary.valid) return false;
	mode_ = mode;
	cleared_ = false;
	static_cast<void>(EvaluateClear(money));
	static_cast<void>(EvaluateSeason(summary));
	return true;
}

int FarmProgressionSystem::GetRemainingMoney(int currentMoney) const noexcept
{
	if (currentMoney >= targetMoney_) {
		return 0;
	}
	return targetMoney_ - (std::max)(0, currentMoney);
}

int FarmProgressionSystem::GetRequiredCropCount(
	int currentMoney, int cropSellPrice) const noexcept
{
	const int remainingMoney = GetRemainingMoney(currentMoney);
	if (remainingMoney <= 0) {
		return 0;
	}
	if (cropSellPrice <= 0) {
		return -1;
	}
	const long long requiredCount =
		(static_cast<long long>(remainingMoney) + cropSellPrice - 1) / cropSellPrice;
	return requiredCount <= static_cast<long long>((std::numeric_limits<int>::max)())
		? static_cast<int>(requiredCount)
		: -1;
}

float FarmProgressionSystem::GetProgress(int currentMoney, int day) const noexcept
{
	if (IsContestSeason()) {
		if (cleared_) return 1.0f;
		return std::clamp(static_cast<float>((std::max)(1, day) - 1) /
			FarmContestEntrySystem::kContestDays.back(), 0.0f, 1.0f);
	}
	const int safeMoney = (std::max)(0, currentMoney);
	return std::clamp(
		static_cast<float>(safeMoney) / static_cast<float>(targetMoney_),
		0.0f,
		1.0f);
}

FarmProgressionSystem::Snapshot FarmProgressionSystem::CaptureSnapshot() const noexcept
{
	return { targetMoney_, cleared_, mode_ };
}

bool FarmProgressionSystem::RestoreSnapshot(const Snapshot& snapshot) noexcept
{
	if (snapshot.targetMoney <= 0 || !ValidMode(snapshot.mode)) {
		return false;
	}
	targetMoney_ = snapshot.targetMoney;
	cleared_ = snapshot.cleared;
	mode_ = snapshot.mode;
	return true;
}
