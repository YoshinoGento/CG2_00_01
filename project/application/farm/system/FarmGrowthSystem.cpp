#include "farm/system/FarmGrowthSystem.h"

#include "farm/core/FarmGrid.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr float kDryGrowthPerSecond = 1.0f / 25.0f;
constexpr float kWetGrowthPerSecond = 1.0f / 10.0f;
constexpr float kMoistureDecayPerSecond = 1.0f / 60.0f;
}

void FarmGrowthSystem::Update(farm::FarmGrid& grid, float deltaTime, float timeScale) const
{
	if (!std::isfinite(deltaTime) || !std::isfinite(timeScale) ||
		deltaTime <= 0.0f || timeScale <= 0.0f) {
		return;
	}

	const float scaledDeltaTime = deltaTime * timeScale;
	for (int tileIndex = 0; tileIndex < grid.GetTileCount(); ++tileIndex) {
		farm::FarmTile* tile = grid.GetMutableTile(tileIndex);
		if (tile == nullptr) {
			continue;
		}

		tile->moisture = std::clamp(tile->moisture, 0.0f, 1.0f);
		tile->growth = std::clamp(tile->growth, 0.0f, 1.0f);
		if (tile->state != farm::FarmTileState::Planted ||
			tile->crop == farm::CropType::None || farm::IsHarvestReady(*tile)) {
			continue;
		}

		const float growthPerSecond = kDryGrowthPerSecond +
			(kWetGrowthPerSecond - kDryGrowthPerSecond) * tile->moisture;
		tile->growth = std::clamp(
			tile->growth + growthPerSecond * scaledDeltaTime, 0.0f, 1.0f);
		tile->moisture = std::clamp(
			tile->moisture - kMoistureDecayPerSecond * scaledDeltaTime, 0.0f, 1.0f);
	}
}
