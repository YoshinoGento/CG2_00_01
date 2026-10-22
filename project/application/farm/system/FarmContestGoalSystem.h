#pragma once
#include "farm/system/FarmContestSeasonSystem.h"
#include "farm/system/FarmStageAccessSystem.h"

enum class FarmContestGoalStatus { Unknown, Upcoming, Open, Passed, Failed, Missed };
struct FarmContestGoal {
    FarmContestGoalStatus status = FarmContestGoalStatus::Unknown;
    int target = 0;
    int shortfall = 0;
};
using FarmContestGoals = std::array<FarmContestGoal, kFarmStageRulesV1.size()>;

// Read-only verdicts; never reinterpret season-ended as success or overwrite recorded points.
class FarmContestGoalSystem final {
public:
    [[nodiscard]] static FarmContestGoals Evaluate(int day,
        const FarmEconomySystem::ContestResults& results, bool staged) noexcept {
        FarmContestGoals goals{};
        const auto summary = FarmContestSeasonSystem::Evaluate(day, results);
        if (!staged || !summary.valid) return goals;
        for (std::size_t i = 0; i < goals.size(); ++i) {
            auto& goal = goals[i];
            goal.target = kFarmStageRulesV1[i].passingPoints;
            switch (summary.events[i]) {
            case FarmContestEventStatus::Upcoming: goal.status = FarmContestGoalStatus::Upcoming; break;
            case FarmContestEventStatus::Open: goal.status = FarmContestGoalStatus::Open; break;
            case FarmContestEventStatus::Missed: goal.status = FarmContestGoalStatus::Missed; break;
            case FarmContestEventStatus::Submitted: {
                const int points = results[i].qualityPoints + results[i].sizePoints;
                goal.shortfall = (std::max)(0, goal.target - points);
                goal.status = goal.shortfall == 0 ? FarmContestGoalStatus::Passed : FarmContestGoalStatus::Failed;
                break;
            }
            default: return {};
            }
        }
        return goals;
    }
};
