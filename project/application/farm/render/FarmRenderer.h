#pragma once

namespace farm {

class FarmGrid;

class FarmRenderer {
public:
	void SetVisible(bool visible) { visible_ = visible; }
	bool IsVisible() const { return visible_; }

	void Draw(const FarmGrid& grid);
	int GetLastDrawTileCount() const { return lastDrawTileCount_; }

private:
	bool visible_ = true;
	int lastDrawTileCount_ = 0;
};

} // namespace farm
