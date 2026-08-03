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

	if (width <= 0 || height <= 0) {
		return false;
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

bool FarmGrid::SelectTile(int index)
{
	return SetSelectedIndex(index);
}

void FarmGrid::UpdateGrowth(float)
{
	// Growth is owned by FarmGrowthSystem. This legacy entry point is intentionally inert.
}

int FarmGrid::GetSelectedIndex() const
{
	if (!IsValid()) {
		return -1;
	}

	return selectedY_ * width_ + selectedX_;
}

FarmTile* FarmGrid::GetSelectedTile()
{
	return GetMutableSelectedTile();
}

const FarmTile* FarmGrid::GetSelectedTile() const
{
	return GetTile(GetSelectedIndex());
}

FarmTile* FarmGrid::GetTile(int index)
{
	return GetMutableTile(index);
}

const FarmTile* FarmGrid::GetTile(int index) const
{
	if (index < 0 || index >= GetTileCount()) {
		return nullptr;
	}

	return &tiles_[static_cast<std::size_t>(index)];
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
	if (destination == nullptr || !std::isfinite(tile.moisture) || !std::isfinite(tile.growth)) {
		return false;
	}
	if (tile.heightLevel < 0 || tile.heightLevel > 2) {
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
	++generation_;
	return true;
}

bool FarmGrid::IsValid() const
{
	return width_ > 0 && height_ > 0 && !tiles_.empty();
}

} // namespace farm
