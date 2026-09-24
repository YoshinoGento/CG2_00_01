#include "farm/system/FarmTimeAdvanceSystem.h"
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using Advance = FarmTimeAdvanceSystem;
using Reason = Advance::StopReason;
void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
struct Fixture {
    farm::FarmGrid grid;
    farm::FarmIrrigationSystem irrigation;
    FarmGrowthSystem growth;
    FarmDateSystem date;
    FarmContestDaySystem contest;
    FarmEconomySystem economy;
    FarmProgressionSystem progression;
    Advance advance;
    Fixture() {
        Require(grid.Initialize(5, 4), "grid");
        irrigation.Initialize(); growth.Initialize(); date.Initialize(); economy.Initialize();
        progression.Initialize({}, FarmProgressionMode::ContestSeason);
    }
    Advance::Context Context() { return {grid, irrigation, growth, date, contest, economy, progression}; }
    void Crop(farm::CropType crop, float water, float growthValue = 0.2f, int index = 19) {
        farm::FarmTile tile;
        tile.state = farm::FarmTileState::Planted; tile.crop = crop;
        tile.moisture = water; tile.growth = growthValue;
        Require(grid.SetTile(index, tile), "crop fixture");
    }
    void RunToStop() {
        int batches = 0;
        while (advance.GetStatus().active) {
            const int steps = advance.AdvanceBatch(Context(), false);
            Require(steps >= 0 && steps <= Advance::kStepsPerBatch, "batch bound");
            Require(++batches <= Advance::kSessionStepLimit, "termination");
        }
    }
};
void SameClock(const FarmDateSystem::Snapshot& a, const FarmDateSystem::Snapshot& b) {
    Require(a.day == b.day && a.elapsedSecondsInDay == b.elapsedSecondsInDay && a.timeScale == b.timeScale,
        "clock mismatch");
}
void CheckBlocked(Fixture& f, Reason reason, int index = -1) {
    const auto before = f.date.CaptureSnapshot();
    Require(!f.advance.Start(f.Context(), false), "unsafe start accepted");
    Require(f.advance.GetStatus().reason == reason && f.advance.GetStatus().tileIndex == index, "stop explanation");
    Require(f.advance.AdvanceBatch(f.Context(), false) == 0, "stopped session advanced");
    SameClock(before, f.date.CaptureSnapshot());
}
}

int main() try {
    for (const float speed : {1.0f, 2.0f, 4.0f}) {
        for (const int day : {10, 20, 30}) {
            Fixture f;
            Require(f.date.RestoreSnapshot({day - 1, 59.99f, speed}), "near event fixture");
            Require(f.advance.Start(f.Context(), false), "event start");
            f.RunToStop();
            Require(f.date.GetDay() == day && f.date.GetElapsedSecondsInDay() == 0, "event overshoot");
            Require(f.advance.GetStatus().reason == Reason::ContestDay && f.contest.PendingDay() == day, "event reason");
            const auto flow = f.irrigation.GetLastStep(f.grid);
            Require(flow.valid && flow.simulatedSeconds <= 0.01001, "irrigation stepped beyond date boundary");
            CheckBlocked(f, Reason::ContestDay);
        }
        for (const auto crop : {farm::CropType::TestCrop, farm::CropType::Carrot, farm::CropType::Tomato, farm::CropType::Pumpkin}) {
            Fixture profileFixture; profileFixture.Crop(crop, .55f);
            const auto forecast=profileFixture.growth.Evaluate(*profileFixture.grid.GetTile(19));
            const float minimum=forecast.goodMoistureMinimum;
            const float maximum=forecast.goodMoistureMaximum;
            for (const float moisture : {0.0f, minimum - 0.001f, maximum + 0.001f, 1.0f}) {
                Fixture f; f.date.SetTimeScale(speed); f.Crop(crop, moisture);
                CheckBlocked(f, moisture < minimum ? Reason::WaterLow : Reason::WaterExcess, 19);
            }
            {
                Fixture f; f.Crop(crop, 0.55f, 1.0f);
                CheckBlocked(f, Reason::HarvestReady, 19);
            }
            {
                Fixture f; f.date.SetTimeScale(speed); f.Crop(crop, 0.55f, 0.9999f);
                Require(f.advance.Start(f.Context(), false), "near-ready start");
                Require(f.advance.AdvanceBatch(f.Context(), false) == 1, "harvest stop not immediate");
                Require(f.advance.GetStatus().reason == Reason::HarvestReady &&
                    f.advance.GetStatus().tileIndex == 19, "harvest reason");
            }
            {
                Fixture f; f.date.SetTimeScale(speed); f.Crop(crop, minimum + 0.000001f);
                Require(f.advance.Start(f.Context(), false), "near-low start");
                Require(f.advance.AdvanceBatch(f.Context(), false) == 1, "low stop not immediate");
                Require(f.advance.GetStatus().reason == Reason::WaterLow, "low reason");
            }
            {
                Fixture f; f.date.SetTimeScale(speed); f.Crop(crop, maximum - 0.000001f, 0.2f, 2);
                farm::FarmTile source; source.feature = farm::FarmTileFeature::WaterSource; source.waterAmount = 1;
                farm::FarmTile canal; canal.feature = farm::FarmTileFeature::Canal; canal.waterAmount = 1;
                Require(f.grid.SetTile(0, source) && f.grid.SetTile(1, canal), "water fixture");
                Require(f.advance.Start(f.Context(), false), "near-excess start");
                Require(f.advance.AdvanceBatch(f.Context(), false) == 1, "excess stop not immediate");
                Require(f.advance.GetStatus().reason == Reason::WaterExcess &&
                    f.advance.GetStatus().tileIndex == 2, "excess reason");
            }
            {
                Fixture batch, ordinary;
                const float midpoint=(minimum+maximum)*.5f;
                batch.Crop(crop, midpoint); ordinary.Crop(crop, midpoint);
                batch.date.SetTimeScale(speed); ordinary.date.SetTimeScale(speed);
                Require(batch.advance.Start(batch.Context(), false), "equivalence start");
                const int steps = batch.advance.AdvanceBatch(batch.Context(), false);
                Require(steps == Advance::kStepsPerBatch && batch.advance.GetStatus().active, "batch budget");
                for (int i = 0; i < steps; ++i) {
                    ordinary.irrigation.UpdateWater(ordinary.grid, Advance::kStepSeconds, speed);
                    ordinary.growth.Update(ordinary.grid, Advance::kStepSeconds, speed);
                    ordinary.contest.Advance(ordinary.date, Advance::kStepSeconds, ordinary.economy.GetContestResults(), 31);
                }
                SameClock(batch.date.CaptureSnapshot(), ordinary.date.CaptureSnapshot());
                const auto* a = batch.grid.GetTile(19); const auto* b = ordinary.grid.GetTile(19);
                Require(a && b && a->growth == b->growth && a->moisture == b->moisture &&
                    a->careHistory.GetObservedSeconds() == b->careHistory.GetObservedSeconds(), "growth equivalence");
                const auto before = batch.date.CaptureSnapshot();
                Require(!batch.advance.Start(batch.Context(), false), "double start accepted");
                batch.advance.Cancel();
                Require(batch.advance.GetStatus().reason == Reason::Cancelled &&
                    batch.advance.AdvanceBatch(batch.Context(), false) == 0, "cancel ignored");
                SameClock(before, batch.date.CaptureSnapshot());
            }
        }
    }
    for (int change = 0; change < 4; ++change) {
        Fixture f;
        Require(f.advance.Start(f.Context(), false), "invalidation start");
        if (change == 0) { farm::FarmGrid::Snapshot saved; f.grid.CaptureSnapshot(saved); Require(f.grid.RestoreSnapshot(saved), "restore"); }
        if (change == 1) f.date.AdvanceOneDay();
        if (change == 2) f.date.SetTimeScale(2);
        const auto before = f.date.CaptureSnapshot();
        Require(f.advance.AdvanceBatch(f.Context(), change == 3) == 0, "invalidation advanced");
        Require(f.advance.GetStatus().reason == (change == 3 ? Reason::Blocked : Reason::ContextChanged), "invalidation reason");
        SameClock(before, f.date.CaptureSnapshot());
    }
    {
        Fixture f, other;
        Require(f.advance.Start(f.Context(), false), "identity start");
        Require(f.advance.AdvanceBatch(other.Context(), false) == 0 &&
            f.advance.GetStatus().reason == Reason::ContextChanged, "other farm accepted");
    }
    {
        Fixture f;
        Require(!f.advance.Start(f.Context(), true) && f.advance.GetStatus().reason == Reason::Blocked, "blocked start");
        f.advance.Reset();
        Require(f.advance.GetStatus().reason == Reason::None && f.advance.GetStatus().completedSteps == 0, "reset");
        Require(f.date.RestoreSnapshot({30, 59.99f, 4}), "deadline fixture");
        f.contest.Observe(30, f.economy.GetContestResults());
        Require(f.contest.Acknowledge(30), "acknowledge deadline");
        Require(f.advance.Start(f.Context(), false), "deadline start");
        f.RunToStop();
        Require(f.date.GetDay() == 31 && f.progression.IsCleared() &&
            f.advance.GetStatus().reason == Reason::Finished, "deadline did not stop");
        CheckBlocked(f, Reason::Finished);
    }
    {
        Fixture f; f.progression.Initialize({}, FarmProgressionMode::Trial);
        Require(f.date.RestoreSnapshot({31, 0, 1}), "no-target fixture");
        CheckBlocked(f, Reason::NoTarget);
    }
    for (const float value : {std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(), -0.1f, 1.1f}) {
        for (const bool corruptGrowth : {false, true}) {
            Fixture f; f.Crop(farm::CropType::Carrot, 0.55f);
            // Bypass SetTile validation only to model corrupted in-memory input.
            auto* tile = f.grid.GetMutableTile(19);
            Require(tile != nullptr, "corrupt fixture pointer");
            if (corruptGrowth) tile->growth = value;
            else tile->moisture = value;
            CheckBlocked(f, Reason::InvalidState, 19);
        }
    }
    {
        Fixture f; f.Crop(farm::CropType::Carrot, 0.55f);
        auto* tile = f.grid.GetMutableTile(19);
        Require(tile != nullptr, "invalid crop fixture pointer");
        tile->crop = static_cast<farm::CropType>(99);
        CheckBlocked(f, Reason::InvalidState, 19);
    }
    {
        Fixture f;
        Require(f.advance.Start(f.Context(), false), "empty farm start");
        f.RunToStop();
        Require(f.date.GetDay() == 10 && f.advance.GetStatus().reason == Reason::ContestDay, "empty farm long run");
        Require(f.advance.GetStatus().completedSteps < Advance::kSessionStepLimit, "long run bounded");
    }
    std::cout << "PASS: bounded time advance, all speeds/crops, event/crop stops, cancellation, invalidation and ordinary-step equivalence\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << "FAIL: " << error.what() << '\n';
    return 1;
}
