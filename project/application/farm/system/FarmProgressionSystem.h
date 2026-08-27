#pragma once

#include "farm/data/FarmRules.h"

class FarmProgressionSystem final {
public:
	struct Snapshot {
		int targetMoney = 1;
		bool cleared = false;
	};

	void Initialize(const farm::FarmRules& rules = {}) noexcept;
	[[nodiscard]] bool EvaluateClear(int currentMoney) noexcept;

	[[nodiscard]] bool IsCleared() const noexcept { return cleared_; }
	[[nodiscard]] int GetTargetMoney() const noexcept { return targetMoney_; }
	[[nodiscard]] int GetRemainingMoney(int currentMoney) const noexcept;
	[[nodiscard]] int GetRequiredCropCount(int currentMoney, int cropSellPrice) const noexcept;
	[[nodiscard]] float GetProgress(int currentMoney) const noexcept;
	[[nodiscard]] Snapshot CaptureSnapshot() const noexcept;
	bool RestoreSnapshot(const Snapshot& snapshot) noexcept;

private:
	int targetMoney_ = 1;
	bool cleared_ = false;
};
