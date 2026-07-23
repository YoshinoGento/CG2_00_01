#include "farm/core/FarmGrid.h"

#include <algorithm>
#include <cstddef>
#include <cmath>

namespace farm {

bool FarmGrid::Initialize(int width, int height)
{
	++generation_;
	width_ = 0;
	height_ = 0;
	selectedX_ = 0;
	selectedY_ = 0;
	tiles_.clear();
	tiles_.resize(static_cast<size_t>(width * height));
	selectedIndex_ = 0;
	harvestCount_ = 0;

	if (width <= 0 || height <= 0) {
		return false;
	}
	}
}

	width_ = width;
	height_ = height;
	tiles_.assign(static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_), FarmTile{});
	return true;
}

void FarmGrid::MoveSelection(int dx, int dy)
{
	if (!IsValid()) {
		return;
	}
		break;
	case FarmAction::Water:
		if (tile.state == FarmTileState::Tilled || tile.state == FarmTileState::Planted) {
			tile.moisture = rules_.maxMoisture;
			if (tile.state == FarmTileState::Tilled) {
				tile.state = FarmTileState::Watered;
			}
			result.accepted = true;
		}
		break;
	case FarmAction::Plant:
		if (tile.state == FarmTileState::Tilled || tile.state == FarmTileState::Watered) {
			tile.state = FarmTileState::Planted;
			tile.growth = rules_.initialCropGrowth;
			result.accepted = true;
		}
		break;
	case FarmAction::Harvest:
		if (tile.IsHarvestable()) {
			++harvestCount_;
			result.accepted = true;
			result.harvestRare = rules_.rareHarvestInterval > 0 && (harvestCount_ % rules_.rareHarvestInterval) == 0;
			result.harvestPrice = result.harvestRare ? rules_.rareHarvestPrice : rules_.normalHarvestPrice;

	selectedX_ = std::clamp(selectedX_ + dx, 0, width_ - 1);
	selectedY_ = std::clamp(selectedY_ + dy, 0, height_ - 1);
}

bool FarmGrid::SetSelectedIndex(int index)
{
	if (!IsValid() || index < 0 || index >= GetTileCount()) {
		return false;
	}
	selectedX_ = index % width_;
	selectedY_ = index / width_;
	return true;
}

int FarmGrid::GetSelectedIndex() const
{
	if (!IsValid()) {
		return -1;
	}

	return selectedY_ * width_ + selectedX_;
}

const FarmTile* FarmGrid::GetSelectedTile() const
{
	return GetTile(GetSelectedIndex());
}

const FarmTile* FarmGrid::GetTile(int index) const
{
	if (index < 0 || index >= GetTileCount()) {
		return nullptr;
	}

	return &tiles_[static_cast<std::size_t>(index)];
}

bool FarmGrid::IsValid() const
{
	return width_ > 0 && height_ > 0 && !tiles_.empty();
}

FarmTile* FarmGrid::GetMutableSelectedTile()
{
	return GetMutableTile(GetSelectedIndex());
}

FarmTile* FarmGrid::GetMutableTile(int index)
{
	if (index < 0 || index >= GetTileCount()) {
		return nullptr;
	}

	return &tiles_[static_cast<std::size_t>(index)];
}

bool FarmGrid::SetTile(int index, const FarmTile& tile)
{
	FarmTile* destination = GetMutableTile(index);
	if (destination == nullptr) {
		return false;
	}
	*destination = tile;
	return true;
}

void FarmGrid::CaptureSnapshot(Snapshot& snapshot) const
{
	snapshot.width = width_;
	snapshot.height = height_;
	snapshot.selectedX = selectedX_;
	snapshot.selectedY = selectedY_;
	snapshot.tiles = tiles_;
}

bool FarmGrid::RestoreSnapshot(const Snapshot& snapshot)
{
	if (snapshot.width != width_ || snapshot.height != height_ ||
		snapshot.tiles.size() != tiles_.size() || !IsValid()) {
		return false;
	}
	for (const FarmTile& tile : snapshot.tiles) {
		if (tile.heightLevel < 0 || tile.heightLevel > 2 ||
			!std::isfinite(tile.moisture) || !std::isfinite(tile.growth)) {
			return false;
		}
	}
	selectedX_ = std::clamp(snapshot.selectedX, 0, width_ - 1);
	selectedY_ = std::clamp(snapshot.selectedY, 0, height_ - 1);
	tiles_ = snapshot.tiles;
	return true;
}

} // namespace farm
