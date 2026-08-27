#pragma once

#include <cmath>
#include <cstdint>

namespace farm {

enum class FarmTileState {
	Empty,
	Tilled,
	Watered,
	Planted,
	ReadyToHarvest,
};

enum class CropType {
	None,
	TestCrop,
	Carrot,
};

enum class FarmCropGrowthStage : std::uint8_t {
	None,
	Sprout,
	Growing,
	AlmostReady,
	Ready,
};

inline constexpr float kFarmGrowthStageGrowingMinimum = 0.25f;
inline constexpr float kFarmGrowthStageAlmostReadyMinimum = 0.70f;

inline constexpr int kFarmCropTypeCount = 2;

inline int ToCropSlot(CropType crop) noexcept
{
	switch (crop) {
	case CropType::TestCrop:
		return 0;
	case CropType::Carrot:
		return 1;
	case CropType::None:
	default:
		return -1;
	}
}

inline CropType CropTypeFromSlot(int slot) noexcept
{
	switch (slot) {
	case 0:
		return CropType::TestCrop;
	case 1:
		return CropType::Carrot;
	default:
		return CropType::None;
	}
}

inline bool IsPlantableCrop(CropType crop) noexcept
{
	return ToCropSlot(crop) >= 0;
}

struct FarmTile {
	int heightLevel = 0;
	FarmTileState state = FarmTileState::Empty;
	CropType crop = CropType::None;
	float moisture = 0.0f;
	float growth = 0.0f;
};

inline bool IsHarvestReady(const FarmTile& tile)
{
	return tile.state == FarmTileState::Planted &&
		tile.crop != CropType::None && tile.growth >= 1.0f;
}

inline FarmCropGrowthStage GetCropGrowthStage(const FarmTile& tile) noexcept
{
	if ((tile.state != FarmTileState::Planted &&
		tile.state != FarmTileState::ReadyToHarvest) ||
		!IsPlantableCrop(tile.crop) || !std::isfinite(tile.growth)) {
		return FarmCropGrowthStage::None;
	}
	if (tile.state == FarmTileState::ReadyToHarvest || tile.growth >= 1.0f) {
		return FarmCropGrowthStage::Ready;
	}
	if (tile.growth >= kFarmGrowthStageAlmostReadyMinimum) {
		return FarmCropGrowthStage::AlmostReady;
	}
	if (tile.growth >= kFarmGrowthStageGrowingMinimum) {
		return FarmCropGrowthStage::Growing;
	}
	return FarmCropGrowthStage::Sprout;
}

inline const char* ToString(FarmTileState state)
{
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

inline const char* ToString(CropType crop)
{
	switch (crop) {
	case CropType::None:
		return "None";
	case CropType::TestCrop:
		return "TestCrop";
	case CropType::Carrot:
		return "Carrot";
	default:
		return "Unknown";
	}
}

} // namespace farm
