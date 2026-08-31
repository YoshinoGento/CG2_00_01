#pragma once

namespace farm {
class FarmGrid;
}

class FarmGrowthSystem final {
public:
	void Update(farm::FarmGrid& grid, float deltaTime, float timeScale) const;
};
