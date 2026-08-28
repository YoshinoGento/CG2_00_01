#include "farm/system/FarmProgressionSystem.h"

#include <algorithm>
#include <limits>

void FarmProgressionSystem::Initialize(const farm::FarmRules& rules) noexcept
{
	targetMoney_ = (std::max)(1, rules.clearMoneyTarget);
	cleared_ = false;
}

bool FarmProgressionSystem::EvaluateClear(int currentMoney) noexcept
{
	if (cleared_ || currentMoney < targetMoney_) {
		return false;
	}
	cleared_ = true;
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

float FarmProgressionSystem::GetProgress(int currentMoney) const noexcept
{
	const int safeMoney = (std::max)(0, currentMoney);
	return std::clamp(
		static_cast<float>(safeMoney) / static_cast<float>(targetMoney_),
		0.0f,
		1.0f);
}

FarmProgressionSystem::Snapshot FarmProgressionSystem::CaptureSnapshot() const noexcept
{
	return { targetMoney_, cleared_ };
}

bool FarmProgressionSystem::RestoreSnapshot(const Snapshot& snapshot) noexcept
{
	if (snapshot.targetMoney <= 0) {
		return false;
	}
	targetMoney_ = snapshot.targetMoney;
	cleared_ = snapshot.cleared;
	return true;
}
