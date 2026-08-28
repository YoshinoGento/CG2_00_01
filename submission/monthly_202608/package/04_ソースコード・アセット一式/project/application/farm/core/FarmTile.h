#pragma once

#include "farm/core/FarmTypes.h"
#include "math/Matrix.h"

#include <cstdint>

namespace farm {

struct FarmTile {
	FarmGridCoord coord{};
	Vector3 worldPosition = { 0.0f, 0.0f, 0.0f };
	FarmTileState state = FarmTileState::Empty;
	int cropType = 0;
	float growth = 0.0f;
	float moisture = 0.0f;
	uint32_t flags = 0;

	bool HasCrop() const {
		return state == FarmTileState::Planted || state == FarmTileState::ReadyToHarvest;
	}

	bool IsHarvestable() const {
		return state == FarmTileState::ReadyToHarvest;
	}
};

} // namespace farm
