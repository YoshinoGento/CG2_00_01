#include "farm/system/FarmToolActionSystem.h"

#include "farm/core/FarmGrid.h"

#include <algorithm>
#include <memory>
#include <string_view>

namespace {
constexpr float kFullMoisture = 1.0f;
constexpr float kInitialGrowth = 0.0f;
constexpr int kMinimumHeightLevel = 0;
constexpr int kMaximumHeightLevel = 2;

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
		const farm::FarmTile& after, const char* name)
		: grid_(&grid), gridGeneration_(grid.GetGeneration()), tileIndex_(tileIndex),
		before_(before), after_(after), name_(name ? name : "Farm Tile Edit") {}

	bool Execute() override { return Apply(after_); }
	bool Undo() override { return Apply(before_); }
	std::string_view GetName() const noexcept override { return name_; }

private:
	bool Apply(const farm::FarmTile& tile) {
		return grid_ != nullptr && grid_->GetGeneration() == gridGeneration_ &&
			grid_->SetTile(tileIndex_, tile);
	}

	// FarmToolActionSystem history is destroyed before the scene-owned FarmGrid.
	farm::FarmGrid* grid_ = nullptr;
	uint64_t gridGeneration_ = 0;
	int tileIndex_ = -1;
	farm::FarmTile before_{};
	farm::FarmTile after_{};
	std::string_view name_;
};
}

bool FarmToolActionSystem::ApplyTool(farm::FarmGrid& grid, FarmTool tool)
{
	const int tileIndex = grid.GetSelectedIndex();
	const farm::FarmTile* selectedTile = grid.GetTile(tileIndex);
	if (selectedTile == nullptr) {
		return false;
	}
	const farm::FarmTile before = *selectedTile;
	farm::FarmTile after = before;
	const char* commandName = "Farm Tool";

	switch (tool) {
	case FarmTool::Hoe:
		if (before.state != farm::FarmTileState::Empty) {
			return false;
		}
		after.state = farm::FarmTileState::Tilled;
		commandName = "Hoe Tile";
		break;
	case FarmTool::Water:
		if (before.state != farm::FarmTileState::Tilled &&
			before.state != farm::FarmTileState::Planted) {
			return false;
		}
		if (before.moisture >= kFullMoisture) {
			return false;
		}
		after.moisture = kFullMoisture;
		commandName = "Water Tile";
		break;
	case FarmTool::Seed:
		if (before.state != farm::FarmTileState::Tilled) {
			return false;
		}
		after.state = farm::FarmTileState::Planted;
		after.crop = farm::CropType::TestCrop;
		after.growth = kInitialGrowth;
		commandName = "Plant Seed";
		break;
	case FarmTool::Harvest:
	case FarmTool::BugNet:
	default:
		return false;
	}
	return CommitTileChange(grid, tileIndex, before, after, commandName);
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
	after.heightLevel = std::clamp(after.heightLevel + 1, kMinimumHeightLevel, kMaximumHeightLevel);
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
	after.heightLevel = std::clamp(after.heightLevel - 1, kMinimumHeightLevel, kMaximumHeightLevel);
	return CommitTileChange(grid, tileIndex, before, after, "Lower Tile");
}

bool FarmToolActionSystem::CommitTileChange(
	farm::FarmGrid& grid, int tileIndex, const farm::FarmTile& before,
	const farm::FarmTile& after, const char* commandName)
{
	if (TilesEqual(before, after)) {
		return false;
	}
	return history_.Execute(std::make_unique<FarmTileEditCommand>(
		grid, tileIndex, before, after, commandName));
}
