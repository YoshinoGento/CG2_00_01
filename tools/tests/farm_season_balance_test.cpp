#include "farm/core/FarmGrid.h"
#include "farm/system/FarmContestDaySystem.h"
#include "farm/system/FarmGrowthSystem.h"
#include "farm/system/FarmIrrigationSystem.h"
#include "farm/system/FarmProgressionSystem.h"
#include "farm/system/FarmToolActionSystem.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <stdexcept>

namespace {
void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
enum class Care { Managed, Dry, Canal };
constexpr std::array<const char*, 3> kCareNames{"managed", "dry", "canal"};
constexpr float kTickSeconds = 1.0f / 60.0f;
constexpr int kMaximumTicks = 120000;
constexpr int kField = 2;

struct Outcome {
    int money = 0;
    int points = 0;
    int watering = 0;
    int purchases = 0;
    int harvested = 0;
    double growingSeconds = 0;
    double excessSeconds = 0;
    double runningSeconds = 0;
    FarmContestRating rating = FarmContestRating::Unrated;
};

// Scripted inputs only. Never assign growth, moisture, date, quality, size or inventory directly.
class SeasonRun {
public:
    SeasonRun(farm::CropType crop, Care care, float speed) : crop_(crop), care_(care) {
        Require(grid_.Initialize(5, 4), "grid init");
        economy_.Initialize(); tools_.Initialize(); growth_.Initialize(); water_.Initialize(); date_.Initialize();
        progression_.Initialize({}, FarmProgressionMode::ContestSeason);
        date_.SetTimeScale(speed);
        if (care_ == Care::Canal) {
            Require(grid_.SelectTile(0), "source select");
            if (crop_ == farm::CropType::Carrot) Require(tools_.RaiseSelectedTile(grid_), "source height");
            Require(tools_.ToggleSelectedWaterSource(grid_), "source placement");
            Require(grid_.SelectTile(1), "canal select");
            if (crop_ == farm::CropType::Carrot) Require(tools_.RaiseSelectedTile(grid_), "canal height");
            Require(tools_.ToggleSelectedCanal(grid_), "canal placement");
        }
        Require(grid_.SelectTile(kField), "field select");
        if (crop_ == farm::CropType::Carrot && care_ != Care::Dry)
            Require(tools_.RaiseSelectedTile(grid_), "carrot height");
        Apply(FarmTool::Hoe);
    }

    Outcome Run(bool skipLast) {
        for (int period = 0; period < 3; ++period) {
            AdvanceTo(1 + period * 10);
            const int candidate = GrowAndHarvest();
            Require(tools_.CommitHarvestProtection(economy_, candidate, true, economy_.GetInventoryGeneration()), "protect");
            Require(tools_.CommitContestReservation(economy_, candidate, true, economy_.GetInventoryGeneration()), "reserve");
            GrowAndHarvest();
            const auto sold = economy_.SellAll();
            Require(sold.Succeeded() && sold.soldCount == 1 && economy_.GetTotalCropCount() == 1, "protected sale");
            earned_ += sold.earnedMoney;
            tools_.ClearHistory();
            const int contestDay = FarmContestEntrySystem::kContestDays[static_cast<std::size_t>(period)];
            AdvanceTo(contestDay);
            Require(notice_.PendingDay() == contestDay, "event notice absent");
            const auto pausedDate = date_.CaptureSnapshot();
            for (int i = 0; i < 120; ++i) Step();
            Require(date_.GetDay() == pausedDate.day && date_.GetElapsedSecondsInDay() == pausedDate.elapsedSecondsInDay,
                "pending event advanced clock");
            if (skipLast && period == 2) {
                Require(notice_.Acknowledge(contestDay), "skip acknowledgement");
                AdvanceTo(31);
                Require(progression_.IsCleared(), "deadline did not end season");
                break;
            }
            const int moneyBefore = economy_.GetMoney();
            const auto generation = economy_.GetInventoryGeneration();
            Require(!tools_.CommitContestSubmission(economy_, contestDay, candidate, generation - 1), "stale submit accepted");
            Require(tools_.CommitContestSubmission(economy_, contestDay, candidate, generation), "submit");
            Require(!tools_.CommitContestSubmission(economy_, contestDay, candidate, economy_.GetInventoryGeneration()), "duplicate submit");
            Require(economy_.GetTotalCropCount() == 0 && economy_.GetMoney() == moneyBefore, "submission ledger");
            Observe();
            Require(progression_.IsCleared() == (period == 2), "wrong completion day");
        }
        const auto summary = FarmContestSeasonSystem::Evaluate(date_.GetDay(), economy_.GetContestResults());
        Require(summary.valid && summary.finalized && summary.submitted == (skipLast ? 2 : 3), "final summary");
        Require(summary.missed == (skipLast ? 1 : 0), "missed count");
        Require(economy_.GetMoney() == farm::FarmRules{}.initialMoney - spent_ + earned_, "money ledger");
        Require(economy_.GetMoney() >= 0 && economy_.GetTotalSeedCount() == 0, "invalid final funds/seeds");
        const auto stopped = date_.CaptureSnapshot();
        for (int i = 0; i < 120; ++i) Step();
        Require(date_.GetDay() == stopped.day && date_.GetElapsedSecondsInDay() == stopped.elapsedSecondsInDay,
            "end clock advanced");
        outcome_.money = economy_.GetMoney(); outcome_.points = summary.totalPoints; outcome_.rating = summary.rating;
        return outcome_;
    }
private:
    void Apply(FarmTool tool) {
        Require(tools_.ApplyToolDetailed(grid_, tool, crop_, economy_, date_.GetDay()).Succeeded(), "tool action");
    }
    void Observe() {
        notice_.Observe(date_.GetDay(), economy_.GetContestResults());
        static_cast<void>(progression_.EvaluateSeason(FarmContestSeasonSystem::Evaluate(date_.GetDay(), economy_.GetContestResults())));
        Require(!progression_.EvaluateClear(economy_.GetMoney()), "money ended season");
    }
    void Step() {
        Observe();
        if (notice_.PendingDay() || progression_.IsCleared()) return;
        Require(++ticks_ <= kMaximumTicks, "season timed out");
        static_cast<void>(water_.UpdateWater(grid_, kTickSeconds, date_.GetTimeScale()));
        const auto balance = water_.GetLastStep(grid_);
        Require(balance.valid && std::isfinite(balance.balanceError) && std::abs(balance.balanceError) < 0.0001,
            "water conservation");
        const auto* tile = grid_.GetTile(kField);
        Require(tile != nullptr, "field pointer");
        if (care_ == Care::Canal) Require(water_.IsInIrrigationRange(kField), "canal scenario not connected");
        if (tile->crop != farm::CropType::None && tile->growth < 1.0f)
            outcome_.growingSeconds += static_cast<double>(kTickSeconds) * date_.GetTimeScale();
        static_cast<void>(growth_.Update(grid_, kTickSeconds, date_.GetTimeScale()));
        notice_.Advance(date_, kTickSeconds, economy_.GetContestResults(), 31);
        outcome_.runningSeconds += kTickSeconds;
        Observe();
    }
    void AdvanceTo(int day) {
        while (date_.GetDay() < day) {
            Require(!progression_.IsCleared() && !notice_.PendingDay(), "blocked before target day");
            Step();
        }
        Require(date_.GetDay() == day, "day skipped");
    }
    int GrowAndHarvest() {
        const auto bought = economy_.BuySeed(crop_);
        Require(bought.Succeeded(), "cannot afford next seed");
        spent_ += bought.spentMoney; ++outcome_.purchases;
        if (care_ != Care::Dry) Require(tools_.CompostSelectedTile(grid_), "compost");
        if (care_ == Care::Managed) {
            const float target = crop_ == farm::CropType::Carrot ? 0.60f : 0.50f;
            while (grid_.GetSelectedTile()->moisture < target) { Apply(FarmTool::Water); ++outcome_.watering; }
        }
        Apply(FarmTool::Seed);
        const int start = ticks_;
        while (tools_.EvaluateTool(grid_, FarmTool::Harvest, crop_, &economy_).status != FarmToolActionStatus::Harvested) {
            Require(ticks_ - start < 36000 && !notice_.PendingDay(), "growth blocked");
            if (care_ == Care::Managed) {
                const float threshold = crop_ == farm::CropType::Carrot ? 0.45f : 0.35f;
                if (grid_.GetSelectedTile()->moisture < threshold) { Apply(FarmTool::Water); ++outcome_.watering; }
            }
            Step();
        }
        const auto harvested = tools_.ApplyToolDetailed(grid_, FarmTool::Harvest, crop_, economy_, date_.GetDay());
        Require(harvested.Succeeded() && harvested.harvestedTile.has_value(), "harvest action");
        outcome_.excessSeconds += harvested.harvestedTile->careHistory.excessSeconds;
        ++outcome_.harvested;
        const auto* record = economy_.GetHarvestRecord(economy_.GetHarvestRecordCount() - 1);
        Require(record && record->quality.harvestSize.known && record->harvestedDay == date_.GetDay(), "harvest record");
        return record->id;
    }
    farm::CropType crop_;
    Care care_;
    farm::FarmGrid grid_;
    FarmEconomySystem economy_;
    FarmToolActionSystem tools_;
    FarmGrowthSystem growth_;
    farm::FarmIrrigationSystem water_;
    FarmDateSystem date_;
    FarmContestDaySystem notice_;
    FarmProgressionSystem progression_;
    Outcome outcome_;
    int spent_ = 0, earned_ = 0, ticks_ = 0;
};
}

int main() {
    try {
        std::cout << "crop,care,speed,end,money,points,rating,watering,harvests,growing_seconds,excess_seconds,running_seconds\n";
        for (const auto crop : {farm::CropType::TestCrop, farm::CropType::Carrot}) {
            for (const auto care : {Care::Managed, Care::Dry, Care::Canal}) {
                for (const bool skipLast : {false, true}) {
                    Outcome baseline;
                    for (const float speed : {1.0f, 2.0f, 4.0f}) {
                        SeasonRun run(crop, care, speed);
                        const auto result = run.Run(skipLast);
                        if (speed == 1.0f) baseline = result;
                        Require(result.harvested == 6 && result.purchases == 6, "missing core loop");
                        Require(std::abs(result.points - baseline.points) <= 3, "speed changed contest score");
                        Require(std::abs(result.money - baseline.money) <= 6, "speed changed finances");
                        Require(result.rating == baseline.rating, "speed changed rating");
                        if (care == Care::Managed && !skipLast) Require(result.rating == FarmContestRating::S, "S unreachable on managed route");
                        if (care == Care::Canal) Require(result.excessSeconds > 0, "unmeasured excess-water scenario");
                        std::cout << (crop == farm::CropType::Carrot ? "carrot" : "turnip") << ','
                            << kCareNames[static_cast<std::size_t>(care)] << ',' << speed << ',' << (skipLast ? "deadline" : "submit")
                            << ',' << result.money << ',' << result.points << ',' << FarmContestRatingText(result.rating)
                            << ',' << result.watering << ',' << result.harvested << ',' << std::fixed << std::setprecision(2)
                            << result.growingSeconds << ',' << result.excessSeconds << ',' << result.runningSeconds << '\n';
                    }
                }
            }
        }
        std::cout << "PASS: 36 full-season System scenarios; automated evidence, not a human playthrough\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
