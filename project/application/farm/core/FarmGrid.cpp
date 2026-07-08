#include "farm/core/FarmGrid.h"

#include <algorithm>
#include <cstddef>

namespace farm {

bool FarmGrid::Initialize(int width, int height)
{
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

bool FarmGrid::RaiseSelectedTileHeight()
{
	FarmTile* selectedTile = GetMutableSelectedTile();
	if (selectedTile == nullptr) {
		return false;
	}

	const int previousHeight = selectedTile->heightLevel;
	selectedTile->heightLevel = std::clamp(selectedTile->heightLevel + 1, kMinHeightLevel, kMaxHeightLevel);
	return selectedTile->heightLevel != previousHeight;
}

bool FarmGrid::LowerSelectedTileHeight()
{
	FarmTile* selectedTile = GetMutableSelectedTile();
	if (selectedTile == nullptr) {
		return false;
	}

	const int previousHeight = selectedTile->heightLevel;
	selectedTile->heightLevel = std::clamp(selectedTile->heightLevel - 1, kMinHeightLevel, kMaxHeightLevel);
	return selectedTile->heightLevel != previousHeight;
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

} // namespace farm
