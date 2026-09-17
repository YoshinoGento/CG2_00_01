#pragma once
#include "farm/system/FarmEconomySystem.h"
#include <array>

enum class FarmContestEntryIssue {
    Eligible, InvalidDay, SeasonEnded, NoReservation, InvalidRecord,
    UnknownHarvestDay, FutureHarvestDay, OutsidePeriod
};

struct FarmContestEntryResult {
    FarmContestEntryIssue issue = FarmContestEntryIssue::InvalidDay;
    int currentDay = 0;
    int contestDay = 0;
    int firstHarvestDay = 0;
    int daysRemaining = 0;
    int harvestedDay = 0;
    farm::CropType crop = farm::CropType::None;
    [[nodiscard]] bool HasContest() const noexcept { return contestDay > 0; }
};

// Read-only period preview. Eligibility does not submit, consume, or change a reservation.
class FarmContestEntrySystem {
public:
    inline static constexpr std::array<int, 3> kContestDays{10, 20, 30};
    [[nodiscard]] static FarmContestEntryResult Evaluate(int currentDay,
        const FarmEconomySystem::HarvestRecord* reservation) noexcept {
        FarmContestEntryResult result;
        result.currentDay = currentDay;
        if (currentDay < 1) return result;
        int first = 1;
        for (int day : kContestDays) {
            if (currentDay <= day) {
                result.contestDay = day;
                result.firstHarvestDay = first;
                result.daysRemaining = day - currentDay;
                break;
            }
            first = day + 1;
        }
        if (!result.HasContest()) { result.issue = FarmContestEntryIssue::SeasonEnded; return result; }
        if (!reservation) { result.issue = FarmContestEntryIssue::NoReservation; return result; }
        if (!FarmEconomySystem::CanReserveForContest(*reservation) || reservation->harvestedDay < 0) {
            result.issue = FarmContestEntryIssue::InvalidRecord; return result;
        }
        result.crop = reservation->quality.crop;
        result.harvestedDay = reservation->harvestedDay;
        result.issue = result.harvestedDay == 0 ? FarmContestEntryIssue::UnknownHarvestDay
            : result.harvestedDay > currentDay ? FarmContestEntryIssue::FutureHarvestDay
            : result.harvestedDay < result.firstHarvestDay || result.harvestedDay > result.contestDay
                ? FarmContestEntryIssue::OutsidePeriod : FarmContestEntryIssue::Eligible;
        return result;
    }
};
