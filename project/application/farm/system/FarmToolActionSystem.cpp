#include "farm/system/FarmToolActionSystem.h"

#include "farm/core/FarmGrid.h"

namespace {
constexpr float kFullMoisture = 1.0f;
constexpr float kInitialGrowth = 0.0f;
}

bool FarmToolActionSystem::ApplyTool(farm::FarmGrid& grid, FarmTool tool)
{
	switch (tool) {
	case FarmTool::Hoe: {
		farm::FarmTile* selectedTile = grid.GetMutableSelectedTile();
		if (selectedTile == nullptr || selectedTile->state != farm::FarmTileState::Empty) {
			return false;
		}

		selectedTile->state = farm::FarmTileState::Tilled;
		return true;
	}
	case FarmTool::Water: {
		farm::FarmTile* selectedTile = grid.GetMutableSelectedTile();
		if (selectedTile == nullptr) {
			return false;
		}
		if (selectedTile->state != farm::FarmTileState::Tilled &&
			selectedTile->state != farm::FarmTileState::Planted) {
			return false;
		}
		if (selectedTile->moisture >= kFullMoisture) {
			return false;
		}

		selectedTile->moisture = kFullMoisture;
		return true;
	}
	case FarmTool::Seed: {
		farm::FarmTile* selectedTile = grid.GetMutableSelectedTile();
		if (selectedTile == nullptr || selectedTile->state != farm::FarmTileState::Tilled) {
			return false;
		}

		selectedTile->state = farm::FarmTileState::Planted;
		selectedTile->crop = farm::CropType::TestCrop;
		selectedTile->growth = kInitialGrowth;
		return true;
	}
	case FarmTool::Harvest:
	case FarmTool::BugNet:
	default:
		return false;
	}
}
