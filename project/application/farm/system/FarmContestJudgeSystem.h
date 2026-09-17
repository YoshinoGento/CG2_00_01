#pragma once
#include "farm/system/FarmEconomySystem.h"
#include <algorithm>
#include <cmath>

struct FarmContestJudgeRules {
    int qualityPoints = 60;
    int sizePoints = 40;
    float sizeMinimum = 0.5f;
    float sizeMaximum = 2.0f;
    [[nodiscard]] bool IsValid() const noexcept {
        return qualityPoints >= 0 && qualityPoints <= 100 && sizePoints >= 0 && sizePoints <= 100 &&
            qualityPoints + sizePoints == 100 && std::isfinite(sizeMinimum) && std::isfinite(sizeMaximum) &&
            sizeMinimum > 0.0f &&
            sizeMaximum <= FarmCropSizeResult::kMaximumMultiplier && sizeMaximum > sizeMinimum;
    }
};

// Persisted rule version 1 is immutable, even when future preview defaults change.
inline constexpr FarmContestJudgeRules kFarmContestSubmissionRulesV1{60, 40, 0.5f, 2.0f};

enum class FarmContestJudgeIssue { None, NoReservation, InvalidRecord, InvalidRules };
struct FarmContestJudgeResult {
    FarmContestJudgeIssue issue = FarmContestJudgeIssue::NoReservation;
    FarmContestJudgeRules rules{};
    farm::CropType crop = farm::CropType::None;
    int recordId = 0;
    int recordedQuality = 0;
    float recordedSize = 0;
    int qualityPoints = 0, sizePoints = 0, totalPoints = 0;
    bool partialQuality = false;
    [[nodiscard]] bool IsValid() const noexcept { return issue == FarmContestJudgeIssue::None; }
};

// Provisional, read-only scoring. This never submits or consumes the reserved crop.
class FarmContestJudgeSystem {
public:
    [[nodiscard]] static FarmContestJudgeResult Evaluate(const FarmEconomySystem::HarvestRecord* record,
        const FarmContestJudgeRules& rules = {}) noexcept {
        FarmContestJudgeResult result;
        if (!rules.IsValid()) { result.issue = FarmContestJudgeIssue::InvalidRules; return result; }
        result.rules = rules;
        if (!record) return result;
        if (!FarmEconomySystem::CanReserveForContest(*record)) {
            result.issue = FarmContestJudgeIssue::InvalidRecord; return result;
        }
        result.issue = FarmContestJudgeIssue::None;
        result.crop = record->quality.crop;
        result.recordId = record->id;
        result.recordedQuality = record->quality.score;
        result.recordedSize = record->quality.harvestSize.multiplier;
        result.partialQuality = !record->quality.nutrientKnown;
        const double sizeRatio = std::clamp((static_cast<double>(result.recordedSize) - rules.sizeMinimum) /
            (static_cast<double>(rules.sizeMaximum) - rules.sizeMinimum), 0.0, 1.0);
        // Round each contribution before summing, so the displayed breakdown adds up exactly.
        result.qualityPoints = static_cast<int>(std::lround(result.recordedQuality * rules.qualityPoints / 100.0));
        result.sizePoints = static_cast<int>(std::lround(sizeRatio * rules.sizePoints));
        result.totalPoints = result.qualityPoints + result.sizePoints;
        return result;
    }
};
