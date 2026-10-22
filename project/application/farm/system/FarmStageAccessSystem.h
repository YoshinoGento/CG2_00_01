#pragma once
#include "farm/core/FarmTypes.h"
#include "farm/system/FarmDateSystem.h"
#include <array>

struct FarmStageRule {
    int firstDay;
    int contestDay;
    farm::CropType unlockedCrop;
    int maximumHeight;
    int passingPoints;
};
// Schema18 opt-in selects this immutable ruleset. Calibrated by farm_season_balance_test.
inline constexpr std::array<FarmStageRule, 3> kFarmStageRulesV1{{
    {1, 10, farm::CropType::Carrot, 1, 65},
    {11, 20, farm::CropType::Tomato, 2, 75},
    {21, 30, farm::CropType::Pumpkin, 2, 85}
}};

// Persisted opt-in for new campaigns. Legacy farms retain their existing capabilities.
struct FarmStagePolicy { bool enabled = false; };

struct FarmStageAccess {
    bool restricted = false;
    int stage = 2;
    int maximumHeight = kFarmStageRulesV1.back().maximumHeight;
    int nextUnlockDay = 0;
    [[nodiscard]] bool Allows(farm::CropType crop) const noexcept {
        if (!farm::IsPlantableCrop(crop)) return false;
        if (!restricted) return true;
        return crop == farm::CropType::Carrot ||
            (crop == farm::CropType::Tomato && stage >= 1) ||
            (crop == farm::CropType::Pumpkin && stage >= 2);
    }
};

// Scene-owned read-only access service. Dependencies must outlive the service and its consumers.
// Reading the clock at the command boundary avoids stale access after load/mode changes.
class FarmStageAccessSystem final {
public:
    void Bind(const FarmDateSystem& date, const FarmStagePolicy& policy) noexcept { date_ = &date; policy_ = &policy; }
    [[nodiscard]] static FarmStageAccess Evaluate(int day, bool enabled) noexcept {
        if (!enabled) return {};
        const int stage = day >= kFarmStageRulesV1[2].firstDay ? 2 : day >= kFarmStageRulesV1[1].firstDay ? 1 : 0;
        return {true, stage, kFarmStageRulesV1[stage].maximumHeight,
            stage < 2 ? kFarmStageRulesV1[stage + 1].firstDay : 0};
    }
    [[nodiscard]] FarmStageAccess Get() const noexcept {
        return Evaluate(date_ ? date_->GetDay() : 1, policy_ && policy_->enabled);
    }
    [[nodiscard]] static int UnlockDay(farm::CropType crop) noexcept {
        for (const auto& rule : kFarmStageRulesV1) if (crop == rule.unlockedCrop) return rule.firstDay;
        return 0;
    }
private:
    const FarmDateSystem* date_ = nullptr;
    const FarmStagePolicy* policy_ = nullptr;
};
