#pragma once

#include <cstdint>

namespace farm {

enum class FarmTileState : uint8_t {
	Empty,
	Tilled,
	Watered,
	Planted,
	ReadyToHarvest,
};

enum class FarmAction : uint8_t {
	None,
	Till,
	Water,
	Plant,
	Harvest,
};

struct FarmGridCoord {
	int x = 0;
	int z = 0;
};

struct FarmActionResult {
	bool accepted = false;
	FarmAction action = FarmAction::None;
	int tileIndex = -1;
	bool harvestRare = false;
	int harvestPrice = 0;
};

inline const char* ToString(FarmTileState state) {
	switch (state) {
	case FarmTileState::Empty:
		return "Empty";
	case FarmTileState::Tilled:
		return "Tilled";
	case FarmTileState::Watered:
		return "Watered";
	case FarmTileState::Planted:
		return "Planted";
	case FarmTileState::ReadyToHarvest:
		return "ReadyToHarvest";
	default:
		return "Unknown";
	}
}

inline const char* ToString(FarmAction action) {
	switch (action) {
	case FarmAction::None:
		return "None";
	case FarmAction::Till:
		return "Till";
	case FarmAction::Water:
		return "Water";
	case FarmAction::Plant:
		return "Plant";
	case FarmAction::Harvest:
		return "Harvest";
	default:
		return "Unknown";
	}
}

} // namespace farm
