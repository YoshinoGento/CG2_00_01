#include "farm/core/FarmGrid.h"
#include "farm/system/FarmDocumentSystem.h"
#include "farm/system/FarmLayoutSystem.h"
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
constexpr const char* kDocumentName = "給水停止の農場";
constexpr const char* kLayoutName = "給水停止の配置";
void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
void SetIntakes(farm::FarmGrid& grid, bool first) {
    Require(grid.GetTileCount() == 2, "Expected two tiles");
    grid.GetMutableTile(0)->irrigationEnabled = first;
    grid.GetMutableTile(1)->irrigationEnabled = !first;
}
void CheckIntakes(const farm::FarmGrid& grid, bool first) {
    Require(grid.GetTileCount() == 2, "Loaded tile count");
    Require(grid.GetTile(0)->irrigationEnabled == first, "First intake mismatch");
    Require(grid.GetTile(1)->irrigationEnabled != first, "Second intake mismatch");
}
}

int main(int argc, char** argv) {
    try {
        Require(argc == 3, "Usage: restart-test write|read|update|read-updated numeric-run-id");
        const std::string mode = argv[1], runId = argv[2];
        Require(!runId.empty() && runId.size() <= 32 &&
            runId.find_first_not_of("0123456789") == std::string::npos, "Invalid run id");
        Require(mode == "write" || mode == "read" || mode == "update" || mode == "read-updated", "Invalid mode");
        const auto root = std::filesystem::path("generated/codex_checks") / ("intake_restart_" + runId);
        if (mode == "write") Require(!std::filesystem::exists(root), "Fixture already exists");
        else Require(std::filesystem::is_directory(root), "Writer fixture missing");

        farm::FarmGrid grid;
        Require(grid.Initialize(2, 1), "Grid initialization");
        // Readers start with the opposite flags so an ignored load cannot pass.
        SetIntakes(grid, mode != "read-updated");
        FarmEconomySystem economy; economy.Initialize();
        FarmCropSelectionSystem selection; selection.Initialize();
        FarmDateSystem date; date.Initialize();
        FarmProgressionSystem progression; progression.Initialize({}, FarmProgressionMode::ContestSeason);
        FarmDocumentSystem documents;
        Require(documents.Initialize((root / "documents").string(), grid, economy, selection, &date, &progression), "Document initialization");
        FarmLayoutSystem layouts;
        Require(layouts.Initialize((root / "layouts").string()), "Layout initialization");

        if (mode == "write") {
            SetIntakes(grid, false);
            auto* tile = grid.GetMutableTile(0);
            tile->state = farm::FarmTileState::Tilled;
            tile->heightLevel = 1;
            tile->moisture = 0.37f;
            Require(date.RestoreSnapshot({7, 12.5f, 2.f}), "Clock fixture");
            Require(documents.SaveAs(kDocumentName, grid, economy, selection), "Named document save");
            Require(layouts.SaveNew(kLayoutName, grid), "Named layout save");
        } else {
            Require(documents.GetStatus() == FarmDocumentStatus::Loaded && documents.FileExists(), "Startup did not load document");
            Require(documents.GetDisplayName() == kDocumentName && documents.GetDocuments().size() == 1, "Document name/catalog");
            CheckIntakes(grid, mode == "read-updated");
            Require(grid.GetTile(0)->heightLevel == 1 && std::abs(grid.GetTile(0)->moisture - 0.37f) < 0.0001f, "Normal save lost soil state");
            Require(date.GetDay() == 7 && date.GetElapsedSecondsInDay() == 12.5f && date.GetTimeScale() == 2.f, "Normal save lost clock");
            Require(progression.IsContestSeason(), "Normal save lost mode");
            if (mode == "update") {
                SetIntakes(grid, true);
                documents.MarkDirty();
                Require(documents.Save(grid, economy, selection) && !documents.IsDirty(), "Overwrite save");
            }
            farm::FarmGrid layoutGrid;
            Require(layoutGrid.Initialize(2, 1), "Layout grid initialization");
            SetIntakes(layoutGrid, true);
            Require(layouts.Entries().size() == 1 && layouts.Entries()[0].name == kLayoutName, "Layout name/catalog");
            Require(layouts.Load(layouts.Entries()[0].id, layoutGrid), "Layout load");
            CheckIntakes(layoutGrid, false);
            Require(layoutGrid.GetTile(0)->heightLevel == 1 && layoutGrid.GetTile(0)->moisture == 0.f, "Layout must not restore progress");
            CheckIntakes(grid, mode == "update" || mode == "read-updated");
        }
        std::cout << "PASS: separate-process intake " << mode << '\n';
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
