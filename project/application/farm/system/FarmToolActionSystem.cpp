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
constexpr int kHarvestReward = 120;

bool TilesEqual(const farm::FarmTile& left, const farm::FarmTile& right)
{
	return left.heightLevel == right.heightLevel &&
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
		FarmEconomySystem* economySystem, farm::CropType harvestedCrop,
		int harvestedQuantity)
		: grid_(&grid), gridGeneration_(grid.GetGeneration()), tileIndex_(tileIndex),
		before_(before), after_(after), name_(name ? name : "Farm Tile Edit"),
		economySystem_(economySystem), harvestedCrop_(harvestedCrop),
		harvestedQuantity_(harvestedQuantity) {}

	bool Execute() override { return Apply(after_, true); }
	bool Undo() override { return Apply(before_, false); }
	std::string_view GetName() const noexcept override { return name_; }

private:
	bool Apply(const farm::FarmTile& tile, bool addHarvest) {
		if (grid_ == nullptr || grid_->GetGeneration() != gridGeneration_) {
			return false;
		}

		const bool updatesEconomy = economySystem_ != nullptr &&
			harvestedCrop_ != farm::CropType::None && harvestedQuantity_ > 0;
		if (updatesEconomy) {
			const bool economyChanged = addHarvest
				? economySystem_->AddHarvest(harvestedCrop_, harvestedQuantity_)
				: economySystem_->RemoveHarvest(harvestedCrop_, harvestedQuantity_);
			if (!economyChanged) {
				return false;
			}
		}

		if (grid_->SetTile(tileIndex_, tile)) {
			return true;
		}

		if (updatesEconomy) {
			if (addHarvest) {
				static_cast<void>(economySystem_->RemoveHarvest(
					harvestedCrop_, harvestedQuantity_));
			} else {
				static_cast<void>(economySystem_->AddHarvest(
					harvestedCrop_, harvestedQuantity_));
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
	farm::CropType harvestedCrop_ = farm::CropType::None;
	int harvestedQuantity_ = 0;
};
}

bool FarmToolActionSystem::ApplyTool(farm::FarmGrid& grid, FarmTool tool)
{
	return ApplyToolDetailed(grid, tool).Succeeded();
}

FarmToolActionResult FarmToolActionSystem::EvaluateTool(
	const farm::FarmGrid& grid, FarmTool tool) const noexcept
{
	return EvaluateTool(grid, grid.GetSelectedIndex(), tool);
}

FarmToolActionResult FarmToolActionSystem::EvaluateTool(
	const farm::FarmGrid& grid, int tileIndex, FarmTool tool) const noexcept
{
	FarmToolActionResult result{};
	result.tool = tool;
	result.tileIndex = tileIndex;
	const farm::FarmTile* tile = grid.GetTile(tileIndex);
	if (tile == nullptr) {
		result.status = FarmToolActionStatus::InvalidTarget;
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
		result.status = tile->state == farm::FarmTileState::Tilled
			? FarmToolActionStatus::Applied
			: FarmToolActionStatus::InvalidState;
		break;
	case FarmTool::Harvest:
		if (tile->state != farm::FarmTileState::Planted ||
			tile->crop == farm::CropType::None) {
			result.status = FarmToolActionStatus::InvalidState;
		} else if (tile->growth < kHarvestReadyGrowth) {
			result.status = FarmToolActionStatus::NotReady;
		} else {
			result.status = FarmToolActionStatus::Harvested;
			result.reward = kHarvestReward;
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
	farm::FarmGrid& grid, FarmTool tool, FarmEconomySystem* economySystem)
{
	FarmToolActionResult result = EvaluateTool(grid, tool);
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
		after.crop = farm::CropType::TestCrop;
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

	const farm::CropType harvestedCrop = tool == FarmTool::Harvest
		? before.crop
		: farm::CropType::None;
	const int harvestedQuantity = tool == FarmTool::Harvest ? 1 : 0;
	if (!CommitTileChange(
		grid, tileIndex, before, after, commandName,
		economySystem, harvestedCrop, harvestedQuantity)) {
		result.status = FarmToolActionStatus::InvalidState;
		return result;
	}
	if (tool == FarmTool::Harvest) {
		result.status = FarmToolActionStatus::Harvested;
		result.reward = kHarvestReward;
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

bool FarmToolActionSystem::CommitTileChange(
	farm::FarmGrid& grid, int tileIndex, const farm::FarmTile& before,
	const farm::FarmTile& after, const char* commandName,
	FarmEconomySystem* economySystem, farm::CropType harvestedCrop,
	int harvestedQuantity)
{
	if (TilesEqual(before, after)) {
		return false;
	}
	return history_.Execute(std::make_unique<FarmTileEditCommand>(
		grid, tileIndex, before, after, commandName,
		economySystem, harvestedCrop, harvestedQuantity));
}
