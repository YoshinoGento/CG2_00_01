#pragma once
#include "farm/system/FarmContestEntrySystem.h"
#include "farm/system/FarmContestJudgeSystem.h"

enum class FarmContestEventStatus { Unknown, Upcoming, Open, Submitted, Missed };
enum class FarmContestRating { Unrated, D, C, B, A, S };

[[nodiscard]] inline const char* FarmContestRatingText(FarmContestRating rating) noexcept {
    switch (rating) {
    case FarmContestRating::D: return "D";
    case FarmContestRating::C: return "C";
    case FarmContestRating::B: return "B";
    case FarmContestRating::A: return "A";
    case FarmContestRating::S: return "S";
    default: return "--";
    }
}

struct FarmContestRatingThreshold { FarmContestRating rating; int points; };
// Provisional contest-only evaluation, not farm rank or a leaderboard placement.
inline constexpr std::array<FarmContestRatingThreshold, 5> kFarmContestRatingV1{{
    {FarmContestRating::S, 270}, {FarmContestRating::A, 240},
    {FarmContestRating::B, 180}, {FarmContestRating::C, 120}, {FarmContestRating::D, 0}
}};

struct FarmContestSeasonSummary {
    std::array<FarmContestEventStatus, 3> events{};
    int submitted = 0;
    int missed = 0;
    int totalPoints = 0;
    bool valid = false;
    bool finalized = false;
    FarmContestRating rating = FarmContestRating::Unrated;
    FarmContestRating nextRating = FarmContestRating::Unrated;
    int pointsToNextRating = 0;
};

// Derived from validated save state, never persisted separately or used to consume inventory.
class FarmContestSeasonSystem final {
public:
    [[nodiscard]] static FarmContestSeasonSummary Evaluate(int day,
        const FarmEconomySystem::ContestResults& results) noexcept {
        FarmContestSeasonSummary summary;
        if (day < 1) return summary;
        for (std::size_t i = 0; i < results.size(); ++i) {
            const int eventDay = FarmContestEntrySystem::kContestDays[i];
            const auto& result = results[i];
            if (result.contestDay != 0) {
                if (result.contestDay != eventDay || eventDay > day ||
                    result.qualityPoints < 0 || result.qualityPoints > kFarmContestSubmissionRulesV1.qualityPoints ||
                    result.sizePoints < 0 || result.sizePoints > kFarmContestSubmissionRulesV1.sizePoints)
                    return {};
                summary.events[i] = FarmContestEventStatus::Submitted;
                ++summary.submitted;
                summary.totalPoints += result.qualityPoints + result.sizePoints;
            } else if (day > eventDay) {
                summary.events[i] = FarmContestEventStatus::Missed;
                ++summary.missed;
            } else {
                summary.events[i] = day == eventDay ? FarmContestEventStatus::Open : FarmContestEventStatus::Upcoming;
            }
        }
        summary.valid = true;
        summary.finalized = day > FarmContestEntrySystem::kContestDays.back() ||
            summary.events.back() == FarmContestEventStatus::Submitted;
        if (summary.finalized && summary.submitted > 0) {
            for (std::size_t i = 0; i < kFarmContestRatingV1.size(); ++i) {
                if (summary.totalPoints < kFarmContestRatingV1[i].points) continue;
                summary.rating = kFarmContestRatingV1[i].rating;
                if (i > 0) {
                    summary.nextRating = kFarmContestRatingV1[i - 1].rating;
                    summary.pointsToNextRating = kFarmContestRatingV1[i - 1].points - summary.totalPoints;
                }
                break;
            }
        }
        return summary;
    }
};
