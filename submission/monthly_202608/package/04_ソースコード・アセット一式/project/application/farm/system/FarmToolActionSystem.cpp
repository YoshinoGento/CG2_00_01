#include "farm/system/FarmToolActionSystem.h"

#include "farm/core/FarmGrid.h"
#include "farm/system/FarmEconomySystem.h"

#include <algorithm>
#include <memory>
#include <string_view>

namespace {
constexpr float kFullMoisture = 1.0f;
constexpr float kInitialGrowth = 0.0f;
constexpr float kHarvestReadyGrowth = 1.0f;
constexpr float kHarvestMoistureCost = 0.25f;

bool TilesEqual(const farm::FarmTile& left, const farm::FarmTile& right)
{
	return left.heightLevel == right.heightLevel &&
		left.feature == right.feature &&
		left.state == right.state &&
		left.crop == right.crop &&
		left.moisture == right.moisture &&
		left.growth == right.growth;
}

class FarmTileEditCommand final : public IUndoableCommand {
public:
	FarmTileEditCommand(
		farm::FarmGrid& grid, int tileIndex, const farm::FarmTile& before,
		const farm::FarmTile& after, const char* name,
		FarmEconomySystem* economySystem,
		const FarmCropQualityResult& harvestedQuality,
		int harvestedQuantity, farm::CropType plantedCrop, int plantedQuantity)
		: grid_(&grid), gridGeneration_(grid.GetGeneration()), tileIndex_(tileIndex),
		before_(before), after_(after), name_(name ? name : "Farm Tile Edit"),
		economySystem_(economySystem), harvestedQuality_(harvestedQuality),
		harvestedQuantity_(harvestedQuantity), plantedCrop_(plantedCrop),
		plantedQuantity_(plantedQuantity),
		previousLastHarvestQuality_(economySystem != nullptr
			? economySystem->GetLastHarvestQuality() : FarmCropQualityResult{}) {}

	bool Execute() override { return Apply(after_, true); }
	bool Undo() override { return Apply(before_, false); }
	std::string_view GetName() const noexcept override { return name_; }

private:
	bool Apply(const farm::FarmTile& tile, bool addHarvest) {
		if (grid_ == nullptr || grid_->GetGeneration() != gridGeneration_) {
			return false;
		}

		const bool updatesHarvest = economySystem_ != nullptr &&
			harvestedQuality_.IsValid() && harvestedQuantity_ > 0;
		const bool updatesSeed = economySystem_ != nullptr &&
			plantedCrop_ != farm::CropType::None && plantedQuantity_ > 0;
		if (updatesHarvest) {
			const bool harvestChanged = addHarvest
				? economySystem_->AddHarvest(harvestedQuality_, harvestedQuantity_)
				: economySystem_->RemoveHarvest(
					harvestedQuality_, harvestedQuantity_, previousLastHarvestQuality_);
			if (!harvestChanged) {
				return false;
			}
		}
		if (updatesSeed) {
			const bool seedChanged = addHarvest
				? economySystem_->RemoveSeed(plantedCrop_, plantedQuantity_)
				: economySystem_->AddSeed(plantedCrop_, plantedQuantity_);
			if (!seedChanged) {
				if (updatesHarvest) {
					if (addHarvest) {
						static_cast<void>(economySystem_->RemoveHarvest(
							harvestedQuality_, harvestedQuantity_,
							previousLastHarvestQuality_));
					} else {
						static_cast<void>(economySystem_->AddHarvest(
							harvestedQuality_, harvestedQuantity_));
					}
				}
				return false;
			}
		}

		if (grid_->SetTile(tileIndex_, tile)) {
			return true;
		}

		if (updatesSeed) {
			if (addHarvest) {
				static_cast<void>(economySystem_->AddSeed(plantedCrop_, plantedQuantity_));
			} else {
				static_cast<void>(economySystem_->RemoveSeed(plantedCrop_, plantedQuantity_));
			}
		}
		if (updatesHarvest) {
			if (addHarvest) {
				static_cast<void>(economySystem_->RemoveHarvest(
					harvestedQuality_, harvestedQuantity_,
					previousLastHarvestQuality_));
			} else {
				static_cast<void>(economySystem_->AddHarvest(
					harvestedQuality_, harvestedQuantity_));
			}
		}
		return false;
	}

	// FarmToolActionSystem history is destroyed before the scene-owned FarmGrid.
	farm::FarmGrid* grid_ = nullptr;
	uint64_t gridGeneration_ = 0;
	int tileIndex_ = -1;
	farm::FarmTile before_{};
	farm::FarmTile after_{};
	std::string_view name_;
	FarmEconomySystem* economySystem_ = nullptr;
	FarmCropQualityResult harvestedQuality_{};
	int harvestedQuantity_ = 0;
	farm::CropType plantedCrop_ = farm::CropType::None;
	int plantedQuantity_ = 0;
	FarmCropQualityResult previousLastHarvestQuality_{};
};
}

void FarmToolActionSystem::Initialize(const farm::FarmRules& rules) noexcept
{
	cropQualitySystem_.Initialize(rules);
	history_.Clear();
}

FarmCropQualityResult FarmToolActionSystem::EvaluateHarvestQuality(
	const farm::FarmTile& tile) const noexcept
{
	return cropQualitySystem_.Evaluate(tile);
}

bool FarmToolActionSystem::ApplyTool(
	farm::FarmGrid& grid, FarmTool tool, farm::CropType selectedCrop,
	FarmEconomySystem& economySystem)
{
	return ApplyToolDetailed(grid, tool, selectedCrop, economySystem).Succeeded();
}

FarmToolActionResult FarmToolActionSystem::EvaluateTool(
	const farm::FarmGrid& grid, FarmTool tool,
	farm::CropType selectedCrop,
	const FarmEconomySystem* economySystem) const noexcept
{
	return EvaluateTool(grid, grid.GetSelectedIndex(), tool, selectedCrop, economySystem);
}

FarmToolActionResult FarmToolActionSystem::EvaluateTool(
	const farm::FarmGrid& grid, int tileIndex, FarmTool tool,
	farm::CropType selectedCrop,
	const FarmEconomySystem* economySystem) const noexcept
{
	FarmToolActionResult result{};
	result.tool = tool;
	result.tileIndex = tileIndex;
	const farm::FarmTile* tile = grid.GetTile(tileIndex);
	if (tile == nullptr) {
		result.status = FarmToolActionStatus::InvalidTarget;
		return result;
	}
	if (tile->feature != farm::FarmTileFeature::None) {
		result.status = FarmToolActionStatus::InvalidState;
		return result;
	}

	switch (tool) {
	case FarmTool::Hoe:
		result.status = tile->state == farm::FarmTileState::Empty
			? FarmToolActionStatus::Applied
			: FarmToolActionStatus::InvalidState;
		break;
	case FarmTool::Water:
		if (tile->state != farm::FarmTileState::Tilled &&
			tile->state != farm::FarmTileState::Planted) {
			result.status = FarmToolActionStatus::InvalidState;
		} else {
			result.status = tile->moisture >= kFullMoisture
				? FarmToolActionStatus::AlreadyWatered
				: FarmToolActionStatus::Applied;
		}
		break;
	case FarmTool::Seed:
		if (!farm::IsPlantableCrop(selectedCrop)) {
			result.status = FarmToolActionStatus::InvalidState;
		} else if (tile->state != farm::FarmTileState::Tilled) {
			result.status = FarmToolActionStatus::InvalidState;
		} else if (economySystem != nullptr &&
			economySystem->GetSeedCount(selectedCrop) <= 0) {
			result.status = FarmToolActionStatus::NoSeed;
		} else {
			result.status = FarmToolActionStatus::Applied;
		}
		break;
	case FarmTool::Harvest:
		if (tile->state != farm::FarmTileState::Planted ||
			tile->crop == farm::CropType::None) {
			result.status = FarmToolActionStatus::InvalidState;
		} else if (tile->growth < kHarvestReadyGrowth) {
			result.status = FarmToolActionStatus::NotReady;
		} else {
			result.status = FarmToolActionStatus::Harvested;
			result.harvestQuality = cropQualitySystem_.Evaluate(*tile);
			if (!result.harvestQuality.IsValid()) {
				result.status = FarmToolActionStatus::InvalidState;
			} else {
				result.reward = result.harvestQuality.salePrice;
			}
		}
		break;
	case FarmTool::BugNet:
	default:
		result.status = FarmToolActionStatus::UnsupportedTool;
		break;
	}
	return result;
}

FarmToolActionResult FarmToolActionSystem::ApplyToolDetailed(
	farm::FarmGrid& grid, FarmTool tool, farm::CropType selectedCrop,
	FarmEconomySystem& economySystem)
{
	FarmToolActionResult result = EvaluateTool(
		grid, tool, selectedCrop, &economySystem);
	if (!result.Succeeded()) {
		return result;
	}

	const int tileIndex = result.tileIndex;
	const farm::FarmTile* selectedTile = grid.GetTile(tileIndex);
	if (selectedTile == nullptr) {
		result.status = FarmToolActionStatus::InvalidTarget;
		return result;
	}
	const farm::FarmTile before = *selectedTile;
	farm::FarmTile after = before;
	const char* commandName = "Farm Tool";

	switch (tool) {
	case FarmTool::Hoe:
		after.state = farm::FarmTileState::Tilled;
		commandName = "Hoe Tile";
		break;
	case FarmTool::Water:
		after.moisture = kFullMoisture;
		commandName = "Water Tile";
		break;
	case FarmTool::Seed:
		after.state = farm::FarmTileState::Planted;
		after.crop = selectedCrop;
		after.growth = kInitialGrowth;
		commandName = "Plant Seed";
		break;
	case FarmTool::Harvest:
		after.state = farm::FarmTileState::Tilled;
		after.crop = farm::CropType::None;
		after.growth = kInitialGrowth;
		after.moisture = (std::max)(0.0f, before.moisture - kHarvestMoistureCost);
		commandName = "Harvest Crop";
		break;
	case FarmTool::BugNet:
		result.status = FarmToolActionStatus::UnsupportedTool;
		return result;
	default:
		result.status = FarmToolActionStatus::UnsupportedTool;
		return result;
	}

	const FarmCropQualityResult harvestedQuality = tool == FarmTool::Harvest
		? cropQualitySystem_.Evaluate(before)
		: FarmCropQualityResult{};
	const int harvestedQuantity = tool == FarmTool::Harvest ? 1 : 0;
	const farm::CropType plantedCrop = tool == FarmTool::Seed
		? selectedCrop
		: farm::CropType::None;
	const int plantedQuantity = tool == FarmTool::Seed ? 1 : 0;
	if (!CommitTileChange(
		grid, tileIndex, before, after, commandName,
		&economySystem, harvestedQuality, harvestedQuantity,
		plantedCrop, plantedQuantity)) {
		result.status = FarmToolActionStatus::InvalidState;
		return result;
	}
	if (tool == FarmTool::Harvest) {
		result.status = FarmToolActionStatus::Harvested;
		result.harvestQuality = harvestedQuality;
		result.reward = harvestedQuality.salePrice;
	} else {
		result.status = FarmToolActionStatus::Applied;
	}
	return result;
}

bool FarmToolActionSystem::RaiseSelectedTile(farm::FarmGrid& grid)
{
	const int tileIndex = grid.GetSelectedIndex();
	const farm::FarmTile* selectedTile = grid.GetTile(tileIndex);
	if (selectedTile == nullptr) {
		return false;
	}
	const farm::FarmTile before = *selectedTile;
	farm::FarmTile after = before;
	after.heightLevel = std::clamp(
		after.heightLevel + 1,
		FarmToolActionSystem::kMinimumHeightLevel,
		FarmToolActionSystem::kMaximumHeightLevel);
	return CommitTileChange(grid, tileIndex, before, after, "Raise Tile");
}

bool FarmToolActionSystem::LowerSelectedTile(farm::FarmGrid& grid)
{
	const int tileIndex = grid.GetSelectedIndex();
	const farm::FarmTile* selectedTile = grid.GetTile(tileIndex);
	if (selectedTile == nullptr) {
		return false;
	}
	const farm::FarmTile before = *selectedTile;
	farm::FarmTile after = before;
	after.heightLevel = std::clamp(
		after.heightLevel - 1,
		FarmToolActionSystem::kMinimumHeightLevel,
		FarmToolActionSystem::kMaximumHeightLevel);
	return CommitTileChange(grid, tileIndex, before, after, "Lower Tile");
}

bool FarmToolActionSystem::ToggleSelectedCanal(farm::FarmGrid& grid)
{
	const int tileIndex = grid.GetSelectedIndex();
	const farm::FarmTile* selectedTile = grid.GetTile(tileIndex);
	if (selectedTile == nullptr || !CanToggleCanal(grid, tileIndex)) {
		return false;
	}

	const farm::FarmTile before = *selectedTile;
	farm::FarmTile after = before;
	const char* commandName = "Remove Canal";
	if (before.feature == farm::FarmTileFeature::Canal) {
		after.feature = farm::FarmTileFeature::None;
	} else {
		after.feature = farm::FarmTileFeature::Canal;
		commandName = "Place Canal";
	}
	return CommitTileChange(grid, tileIndex, before, after, commandName);
}

bool FarmToolActionSystem::CanToggleCanal(
	const farm::FarmGrid& grid, int tileIndex) const noexcept
{
	const farm::FarmTile* tile = grid.GetTile(tileIndex);
	if (tile == nullptr || !farm::IsValidFarmTileFeature(tile->feature)) {
		return false;
	}
	if (tile->feature == farm::FarmTileFeature::Canal) {
		return true;
	}
	if (tile->feature != farm::FarmTileFeature::None) {
		return false;
	}
	return tile->state == farm::FarmTileState::Empty &&
		tile->crop == farm::CropType::None &&
		tile->moisture == 0.0f && tile->growth == 0.0f;
}

bool FarmToolActionSystem::ToggleSelectedWaterSource(farm::FarmGrid& grid)
{
	const int tileIndex = grid.GetSelectedIndex();
	const farm::FarmTile* selectedTile = grid.GetTile(tileIndex);
	if (selectedTile == nullptr || !CanToggleWaterSource(grid, tileIndex)) {
		return false;
	}

	const farm::FarmTile before = *selectedTile;
	farm::FarmTile after = before;
	const char* commandName = "Remove Water Source";
	if (before.feature == farm::FarmTileFeature::WaterSource) {
		after.feature = farm::FarmTileFeature::None;
	} else {
		after.feature = farm::FarmTileFeature::WaterSource;
		commandName = "Place Water Source";
	}
	return CommitTileChange(grid, tileIndex, before, after, commandName);
}

bool FarmToolActionSystem::CanToggleWaterSource(
	const farm::FarmGrid& grid, int tileIndex) const noexcept
{
	const farm::FarmTile* tile = grid.GetTile(tileIndex);
	if (tile == nullptr || !farm::IsValidFarmTileFeature(tile->feature)) {
		return false;
	}
	if (tile->feature == farm::FarmTileFeature::WaterSource) {
		return true;
	}
	if (tile->feature != farm::FarmTileFeature::None) {
		return false;
	}
	return tile->state == farm::FarmTileState::Empty &&
		tile->crop == farm::CropType::None &&
		tile->moisture == 0.0f && tile->growth == 0.0f;
}

bool FarmToolActionSystem::CommitTileChange(
	farm::FarmGrid& grid, int tileIndex, const farm::FarmTile& before,
	const farm::FarmTile& after, const char* commandName,
	FarmEconomySystem* economySystem,
	const FarmCropQualityResult& harvestedQuality,
	int harvestedQuantity, farm::CropType plantedCrop, int plantedQuantity)
{
	if (TilesEqual(before, after)) {
		return false;
	}
	return history_.Execute(std::make_unique<FarmTileEditCommand>(
		grid, tileIndex, before, after, commandName,
		economySystem, harvestedQuality, harvestedQuantity,
		plantedCrop, plantedQuantity));
}
