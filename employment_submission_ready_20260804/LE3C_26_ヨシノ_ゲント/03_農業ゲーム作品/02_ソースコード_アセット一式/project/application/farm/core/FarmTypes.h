#pragma once

namespace farm {

enum class FarmTileState {
	Empty,
	Tilled,
	Planted,
};

enum class CropType {
	None,
	TestCrop,
};

struct FarmTile {
	int heightLevel = 0;
	FarmTileState state = FarmTileState::Empty;
	CropType crop = CropType::None;
	float moisture = 0.0f;
	float growth = 0.0f;
};

inline const char* ToString(FarmTileState state)
{
	switch (state) {
	case FarmTileState::Empty:
		return "Empty";
	case FarmTileState::Tilled:
		return "Tilled";
	case FarmTileState::Planted:
		return "Planted";
	default:
		return "Unknown";
	}
}

inline const char* ToString(CropType crop)
{
	switch (crop) {
	case CropType::None:
		return "None";
	case CropType::TestCrop:
		return "TestCrop";
	default:
		return "Unknown";
	}
}

} // namespace farm
