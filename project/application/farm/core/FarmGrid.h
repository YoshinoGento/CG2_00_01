#pragma once

#include "farm/core/FarmTypes.h"

#include <vector>

namespace farm {

class FarmGrid {
public:
	bool Initialize(int width, int height);
	void MoveSelection(int dx, int dy);
	bool RaiseSelectedTileHeight();
	bool LowerSelectedTileHeight();

	int GetWidth() const { return width_; }
	int GetHeight() const { return height_; }
	int GetTileCount() const { return static_cast<int>(tiles_.size()); }
	int GetSelectedIndex() const;

	const FarmTile* GetSelectedTile() const;
	const FarmTile* GetTile(int index) const;
	FarmTile* GetMutableSelectedTile();
	FarmTile* GetMutableTile(int index);

private:
	static constexpr int kMinHeightLevel = 0;
	static constexpr int kMaxHeightLevel = 2;

	bool IsValid() const;

private:
	int width_ = 0;
	int height_ = 0;
	int selectedX_ = 0;
	int selectedY_ = 0;
	std::vector<FarmTile> tiles_;
};

} // namespace farm
