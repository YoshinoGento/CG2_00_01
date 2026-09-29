#include "farm/core/FarmGrid.h"
#include "farm/system/FarmDocumentSystem.h"
#include <array>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace {
void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void Check(const farm::FarmGrid& grid, const FarmEconomySystem& economy,
    const FarmDateSystem& date, const FarmProgressionSystem& progression, FarmProgressionMode mode) {
    Require(grid.GetTileCount() == 20, "Expected 5x4 field");
    Require(date.GetDay() == 1 && date.GetElapsedSecondsInDay() == 0.0f, "Expected fresh Day1");
    Require(economy.GetMoney() == 300 && economy.GetTotalCropCount() == 0, "Unexpected economy");
    constexpr std::array crops{farm::CropType::Carrot, farm::CropType::Tomato, farm::CropType::Pumpkin};
    for (const auto crop : crops) Require(economy.GetSeedCount(crop) == 5, "Expected five seeds per formal crop");
    Require(economy.GetSeedCount(farm::CropType::TestCrop) == 0, "Legacy seeds must be absent");
    Require(economy.GetTotalSeedCount() == 15, "Expected fifteen seeds total");
    Require(progression.GetMode() == mode && !progression.IsCleared(), "Unexpected mode or clear state");
    for (int i = 0; i < grid.GetTileCount(); ++i) {
        const auto* tile = grid.GetTile(i);
        Require(tile && tile->crop == farm::CropType::None && tile->heightLevel == 0,
            "Expected empty flat field");
    }
}
}

int main(int argc, char** argv) {
    try {
        Require(argc == 2 || (argc == 3 && std::string_view(argv[2]) == "--free"),
            "Usage: create_demo_save <new-output-directory> [--free]");
        const auto mode = argc == 3 ? FarmProgressionMode::FreeFarming : FarmProgressionMode::ContestSeason;
        const std::filesystem::path output(argv[1]);
        Require(!std::filesystem::exists(output), "Refusing to overwrite an existing save directory");
        farm::FarmRules rules;
        rules.initialCarrotSeedCount = 5;
        rules.initialTomatoSeedCount = 5;
        rules.initialPumpkinSeedCount = 5;
        farm::FarmGrid grid; Require(grid.Initialize(5, 4), "Grid initialization failed");
        FarmEconomySystem economy; economy.Initialize(rules);
        FarmCropSelectionSystem selection; selection.Initialize();
        FarmDateSystem date; date.Initialize();
        FarmProgressionSystem progression; progression.Initialize(rules, mode);
        FarmDocumentSystem documents;
        Require(documents.Initialize(output.string(), grid, economy, selection, &date, &progression),
            "Document initialization failed");
        Check(grid, economy, date, progression, mode);
        Require(documents.SaveAs(mode == FarmProgressionMode::FreeFarming ? "フリー農業 種各5個" : "企業向け体験用 種各5個",
            grid, economy, selection), "SaveAs failed");
        Require(documents.GetDocuments().size() == 1 && !documents.IsDirty(), "Expected exactly one saved document");

        // Load using normal runtime defaults: the preset must carry its own seeds and mode.
        farm::FarmGrid loadedGrid; Require(loadedGrid.Initialize(5, 4), "Reload grid failed");
        FarmEconomySystem loadedEconomy; loadedEconomy.Initialize();
        FarmCropSelectionSystem loadedSelection; loadedSelection.Initialize();
        FarmDateSystem loadedDate; loadedDate.Initialize();
        FarmProgressionSystem loadedProgression; loadedProgression.Initialize();
        FarmDocumentSystem loadedDocuments;
        Require(loadedDocuments.Initialize(output.string(), loadedGrid, loadedEconomy,
            loadedSelection, &loadedDate, &loadedProgression), "Preset reload failed");
        Check(loadedGrid, loadedEconomy, loadedDate, loadedProgression, mode);
        for (const int money : {300, 540, 1000, (std::numeric_limits<int>::max)()}) {
            Require(!loadedProgression.EvaluateClear(money) && !loadedProgression.IsCleared(),
                "Money must not clear the demo");
        }
        Require(loadedProgression.EvaluateSeason(FarmContestSeasonSystem::Evaluate(31, {})) ==
            (mode == FarmProgressionMode::ContestSeason), "Wrong season deadline policy");
        std::cout << "PASS: one production save, seed counts 5/5/5, Day1/300G, reload, money/day policy correct\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
