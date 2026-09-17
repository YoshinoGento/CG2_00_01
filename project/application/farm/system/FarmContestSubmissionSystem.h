#pragma once
#include "farm/system/FarmContestEntrySystem.h"
#include "farm/system/FarmContestJudgeSystem.h"

enum class FarmContestSubmissionStatus { Ready, NotContestDay, AlreadySubmitted, Ineligible };

class FarmContestSubmissionSystem {
public:
    [[nodiscard]] static FarmContestSubmissionStatus Evaluate(const FarmEconomySystem& economy, int day) noexcept {
        const int slot = Slot(day);
        if (slot < 0) return FarmContestSubmissionStatus::NotContestDay;
        if (economy.GetContestResults()[slot].contestDay != 0) return FarmContestSubmissionStatus::AlreadySubmitted;
        return FarmContestEntrySystem::Evaluate(day, economy.GetContestReservation()).issue == FarmContestEntryIssue::Eligible
            ? FarmContestSubmissionStatus::Ready : FarmContestSubmissionStatus::Ineligible;
    }

    // Validate a value transaction before replacing live inventory and bumping its generation.
    [[nodiscard]] static bool Submit(FarmEconomySystem& economy, int day, int expectedRecordId,
        std::uint64_t expectedGeneration) noexcept {
        if (expectedGeneration != economy.GetInventoryGeneration() || expectedRecordId <= 0 ||
            expectedRecordId != economy.GetContestReservationId() || Evaluate(economy,day) != FarmContestSubmissionStatus::Ready)
            return false;
        const auto* record = economy.GetContestReservation();
        const auto judge = FarmContestJudgeSystem::Evaluate(record, kFarmContestSubmissionRulesV1);
        if (!judge.IsValid()) return false;
        auto next = economy.CaptureSnapshot();
        const int crop = farm::ToCropSlot(record->quality.crop);
        if (crop < 0 || next.cropCounts[crop] < 1 || next.cropValues[crop] < record->quality.salePrice) return false;
        next.contestResults[Slot(day)] = {*record, day, judge.qualityPoints, judge.sizePoints};
        --next.cropCounts[crop];
        next.cropValues[crop] -= record->quality.salePrice;
        std::size_t index = 0;
        while (index < next.harvestRecordCount && next.harvestRecords[index].id != expectedRecordId) ++index;
        if (index == next.harvestRecordCount) return false;
        for (std::size_t i = index + 1; i < next.harvestRecordCount; ++i) next.harvestRecords[i-1] = next.harvestRecords[i];
        next.harvestRecords[--next.harvestRecordCount] = {};
        next.contestReservationId = 0;
        return economy.RestoreSnapshot(next);
    }
private:
    static int Slot(int day) noexcept {
        for (std::size_t i = 0; i < FarmContestEntrySystem::kContestDays.size(); ++i)
            if (day == FarmContestEntrySystem::kContestDays[i]) return static_cast<int>(i);
        return -1;
    }
};
