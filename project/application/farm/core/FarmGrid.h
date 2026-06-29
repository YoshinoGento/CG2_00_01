#pragma once

#include "farm/core/FarmTile.h"
#include "farm/data/FarmRules.h"

#include <vector>

namespace farm {

class FarmGrid {
public:
	void Initialize(const FarmRules& rules = FarmRules{});
	void Reset();

	FarmActionResult ApplyAction(FarmAction action, int tileIndex);
	FarmActionResult ApplyActionToSelected(FarmAction action);

	void UpdateGrowth(float deltaTime);
	bool SelectTile(int index);

	FarmTile* GetTile(int index);
	const FarmTile* GetTile(int index) const;
	FarmTile* GetSelectedTile();
	const FarmTile* GetSelectedTile() const;

	int GetSelectedIndex() const { return selectedIndex_; }
	int GetTileCount() const { return static_cast<int>(tiles_.size()); }
	int GetWidth() const { return rules_.gridWidth; }
	int GetHeight() const { return rules_.gridHeight; }
	const FarmRules& GetRules() const { return rules_; }
	const std::vector<FarmTile>& GetTiles() const { return tiles_; }

private:
	bool IsValidIndex(int index) const;
	int ToIndex(int x, int z) const;

	FarmRules rules_{};
	std::vector<FarmTile> tiles_;
	int selectedIndex_ = 0;
	int harvestCount_ = 0;
};

} // namespace farm
