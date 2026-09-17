#include "farm/system/FarmIrrigationPreviewSystem.h"
#include "farm/system/FarmToolActionSystem.h"
#include <cassert>
#include <iostream>

int main() {
    using namespace farm;
    FarmGrid grid;
    FarmIrrigationPreviewSystem preview;
    FarmToolActionSystem tools;
    preview.Initialize(); tools.Initialize();
    assert(grid.Initialize(5, 4));
    FarmTile special = *grid.GetTile(2);
    special.feature = FarmTileFeature::WaterSource;
    special.waterAmount = 0.6f; special.soilNutrients = 0.42f;
    assert(grid.SetTile(2, special));
    assert(preview.Begin(grid, 0, FarmIrrigationPreviewOperation::RaiseTerrain));
    assert(preview.VisitTerrainTile(grid, 0) && preview.VisitTerrainTile(grid, 4));
    assert(preview.GetChangeCount() == 5);
    assert(preview.VisitTerrainTile(grid, 0) && preview.VisitTerrainTile(grid, 4));
    assert(preview.GetChangeCount() == 5 && grid.GetTile(0)->heightLevel == 0);
    for (int i=0; i<5; ++i) assert(preview.GetPreviewGrid()->GetTile(i)->heightLevel == 1);
    assert(preview.GetPreviewGrid()->GetTile(2)->waterAmount == special.waterAmount);
    assert(preview.GetPreviewGrid()->GetTile(2)->feature == special.feature);
    preview.EndTerrainStroke();
    assert(preview.VisitTerrainTile(grid, 19));
    assert(preview.GetChangeCount() == 6 && !preview.IsTileChanged(9));
    assert(preview.CanConfirm(grid));
    assert(tools.ChangeTerrainBatch(grid, preview.GetChangedTileIndices(), 1));
    assert(!preview.CanConfirm(grid));
    assert(grid.GetTile(19)->heightLevel == 1 && grid.GetTile(9)->heightLevel == 0);
    assert(tools.Undo() && grid.GetTile(0)->heightLevel == 0 && grid.GetTile(19)->heightLevel == 0);
    assert(tools.Redo() && grid.GetTile(0)->heightLevel == 1 && grid.GetTile(19)->heightLevel == 1);
    assert(grid.GetTile(2)->waterAmount == special.waterAmount && grid.GetTile(2)->soilNutrients == special.soilNutrients);
    preview.Cancel();
    assert(preview.Begin(grid, 0, FarmIrrigationPreviewOperation::LowerTerrain));
    assert(preview.VisitTerrainTile(grid, 0) && preview.VisitTerrainTile(grid, 4));
    assert(tools.ChangeTerrainBatch(grid, preview.GetChangedTileIndices(), -1));
    assert(grid.GetTile(0)->heightLevel == 0 && tools.Undo() && grid.GetTile(0)->heightLevel == 1);

    assert(!tools.ChangeTerrainBatch(grid, {0,0}, 1));
    assert(!tools.ChangeTerrainBatch(grid, {0,999}, 1));
    assert(!tools.ChangeTerrainBatch(grid, {-1}, 1));
    assert(!tools.ChangeTerrainBatch(grid, {}, 1));
    assert(!tools.ChangeTerrainBatch(grid, {0}, 100));
    assert(grid.GetTile(0)->heightLevel == 1);
    tools.ClearHistory();
    assert(grid.Initialize(5, 4));
    auto limit = *grid.GetTile(2); limit.heightLevel = FarmToolActionSystem::kMaximumHeightLevel;
    assert(grid.SetTile(2, limit));
    assert(preview.Begin(grid, 0, FarmIrrigationPreviewOperation::RaiseTerrain));
    assert(preview.VisitTerrainTile(grid, 0) && !preview.VisitTerrainTile(grid, 4));
    assert(preview.GetChangeCount() == 1 && preview.GetBlockedTileIndex() == 2 && preview.CanConfirm(grid));
    assert(!tools.ChangeTerrainBatch(grid, {0,2}, 1) && grid.GetTile(0)->heightLevel == 0);
    preview.EndTerrainStroke();
    assert(preview.VisitTerrainTile(grid, 19) && preview.GetPathIssue() == FarmCanalPathIssue::None);
    assert(!preview.VisitTerrainTile(grid, -1) && !preview.VisitTerrainTile(grid, 999));
    assert(preview.VisitTerrainTile(grid, 5) && !preview.IsTileChanged(12));
    preview.Cancel();
    assert(!preview.VisitTerrainTile(grid, 1));

    // All endpoint pairs include diagonal/reverse strokes, repeated visits and cell bounds.
    for (int from=0; from<20; ++from) for (int to=0; to<20; ++to) {
        assert(grid.Initialize(5,4));
        assert(preview.Begin(grid, from, FarmIrrigationPreviewOperation::RaiseTerrain));
        assert(preview.VisitTerrainTile(grid, from) && preview.VisitTerrainTile(grid, to));
        const auto count = preview.GetChangeCount();
        assert(preview.VisitTerrainTile(grid, to) && preview.GetChangeCount() == count);
        assert(count <= 5 && preview.IsTileChanged(from) && preview.IsTileChanged(to));
        for (const int index : preview.GetChangedTileIndices())
            assert(index >= 0 && index < 20 && preview.GetPreviewGrid()->GetTile(index)->heightLevel == 1);
    }
    assert(grid.Initialize(5,4));
    assert(!preview.CanConfirm(grid) && !preview.VisitTerrainTile(grid, 0));
    assert(preview.Begin(grid, 0, FarmIrrigationPreviewOperation::RaiseTerrain));
    for (int i=0; i<20; ++i) { preview.EndTerrainStroke(); assert(preview.VisitTerrainTile(grid, i)); }
    assert(preview.GetChangeCount() == 20 && tools.ChangeTerrainBatch(grid, preview.GetChangedTileIndices(), 1));
    assert(tools.Undo());
    for (int i=0; i<20; ++i) assert(grid.GetTile(i)->heightLevel == 0);
    std::cout << "PASS: height brush interpolation, dedup, limits, stale, preservation, batch undo/redo\n";
}
