#pragma once
#include "farm/core/FarmGrid.h"

// Fixture changes use the same validation boundary as production writes.
template<class Edit>
bool EditFarmTile(farm::FarmGrid& grid, int index, Edit edit) {
    const auto* source = grid.GetTile(index);
    if (!source) return false;
    auto candidate = *source;
    edit(candidate);
    return grid.SetTile(index, candidate);
}
