#include "farm/core/FarmGrid.h"

#include <algorithm>

namespace farm {

void FarmGrid::Initialize(const FarmRules& rules) {
	rules_ = rules;
	Reset();
}

void FarmGrid::Reset() {
	const int width = (std::max)(rules_.gridWidth, 1);
	const int height = (std::max)(rules_.gridHeight, 1);

	tiles_.clear();
	tiles_.resize(static_cast<size_t>(width * height));
	selectedIndex_ = 0;
	harvestCount_ = 0;

	const float originOffsetX = (static_cast<float>(width - 1) * rules_.tileSpacing) * 0.5f;
	const float originOffsetZ = (static_cast<float>(height - 1) * rules_.tileSpacing) * 0.5f;

	for (int z = 0; z < height; ++z) {
		for (int x = 0; x < width; ++x) {
			FarmTile& tile = tiles_[ToIndex(x, z)];
			tile = {};
			tile.coord = { x, z };
			tile.worldPosition = {
				rules_.origin.x + static_cast<float>(x) * rules_.tileSpacing - originOffsetX,
				rules_.origin.y,
				rules_.origin.z + static_cast<float>(z) * rules_.tileSpacing - originOffsetZ,
			};
		}
	}
}

FarmActionResult FarmGrid::ApplyAction(FarmAction action, int tileIndex) {
	FarmActionResult result{};
	result.action = action;
	result.tileIndex = tileIndex;

	if (!IsValidIndex(tileIndex)) {
		return result;
	}

	FarmTile& tile = tiles_[tileIndex];
	switch (action) {
	case FarmAction::Till:
		if (tile.state == FarmTileState::Empty) {
			tile.state = FarmTileState::Tilled;
			tile.growth = 0.0f;
			tile.moisture = 0.0f;
			result.accepted = true;
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

			tile.state = FarmTileState::Tilled;
			tile.growth = 0.0f;
			tile.moisture = rules_.postHarvestMoisture;
		}
		break;
	case FarmAction::None:
	default:
		break;
	}

	return result;
}

FarmActionResult FarmGrid::ApplyActionToSelected(FarmAction action) {
	return ApplyAction(action, selectedIndex_);
}

void FarmGrid::UpdateGrowth(float deltaTime) {
	const float safeDeltaTime = std::clamp(deltaTime, 0.0f, rules_.maxUpdateDeltaTime);
	for (FarmTile& tile : tiles_) {
		tile.moisture = (std::max)(tile.moisture - rules_.moistureDecayPerSecond * safeDeltaTime, 0.0f);
		if (tile.state != FarmTileState::Planted) {
			continue;
		}

		const float growthSpeed =
			tile.moisture > rules_.wetGrowthMoistureThreshold
			? rules_.growthPerSecondWet
			: rules_.growthPerSecondDry;
		tile.growth = std::clamp(tile.growth + growthSpeed * safeDeltaTime, 0.0f, rules_.maxCropGrowth);
		if (tile.growth >= rules_.maxCropGrowth) {
			tile.state = FarmTileState::ReadyToHarvest;
			tile.moisture = (std::max)(tile.moisture, rules_.readyMoistureMinimum);
		}
	}
}

bool FarmGrid::SelectTile(int index) {
	if (!IsValidIndex(index)) {
		return false;
	}
	selectedIndex_ = index;
	return true;
}

FarmTile* FarmGrid::GetTile(int index) {
	if (!IsValidIndex(index)) {
		return nullptr;
	}
	return &tiles_[index];
}

const FarmTile* FarmGrid::GetTile(int index) const {
	if (!IsValidIndex(index)) {
		return nullptr;
	}
	return &tiles_[index];
}

FarmTile* FarmGrid::GetSelectedTile() {
	return GetTile(selectedIndex_);
}

const FarmTile* FarmGrid::GetSelectedTile() const {
	return GetTile(selectedIndex_);
}

bool FarmGrid::IsValidIndex(int index) const {
	return index >= 0 && index < static_cast<int>(tiles_.size());
}

int FarmGrid::ToIndex(int x, int z) const {
	return z * (std::max)(rules_.gridWidth, 1) + x;
}

} // namespace farm
