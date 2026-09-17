#pragma once
#include "farm/system/FarmContestEntrySystem.h"
#include "farm/system/FarmDateSystem.h"

// Session-only notices. Loading a farm starts a new notice session; results remain authoritative.
class FarmContestDaySystem final {
public:
    void Reset() noexcept { acknowledged_ = {}; lastDay_ = 0; pendingDay_ = 0; }
    void Observe(int day, const FarmEconomySystem::ContestResults& results) noexcept {
        if (day < lastDay_) Reset();
        lastDay_ = day;
        pendingDay_ = 0;
        for (std::size_t i = 0; i < acknowledged_.size(); ++i)
            if (day == FarmContestEntrySystem::kContestDays[i] && !acknowledged_[i] && results[i].contestDay == 0)
                pendingDay_ = day;
    }
    [[nodiscard]] int PendingDay() const noexcept { return pendingDay_; }
    [[nodiscard]] bool Acknowledge(int expectedDay) noexcept {
        if (expectedDay <= 0 || expectedDay != pendingDay_) return false;
        for (std::size_t i = 0; i < acknowledged_.size(); ++i)
            if (expectedDay == FarmContestEntrySystem::kContestDays[i]) acknowledged_[i] = true;
        pendingDay_ = 0;
        return true;
    }
    void Advance(FarmDateSystem& date, float deltaTime, const FarmEconomySystem::ContestResults& results, int endAtDay = 0) {
        Observe(date.GetDay(), results);
        if (pendingDay_) return;
        int stopDay = 0;
        for (std::size_t i = 0; i < acknowledged_.size(); ++i) {
            if (FarmContestEntrySystem::kContestDays[i] > date.GetDay() && !acknowledged_[i] && results[i].contestDay == 0) {
                stopDay = FarmContestEntrySystem::kContestDays[i]; break;
            }
        }
        if (endAtDay > date.GetDay() && (stopDay == 0 || endAtDay < stopDay)) stopDay = endAtDay;
        date.Update(deltaTime, stopDay);
        Observe(date.GetDay(), results);
    }
private:
    std::array<bool, 3> acknowledged_{};
    int lastDay_ = 0;
    int pendingDay_ = 0;
};
