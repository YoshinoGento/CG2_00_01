#pragma once

#include "farm/data/FarmRules.h"
#include "farm/core/FarmTypes.h"

#include <cstdint>
#include <vector>

namespace farm {

class FarmGrid;

// Owns finite reservoir flow and soil delivery; topology is potential reachability.
class FarmIrrigationSystem final {
public:
	void Initialize(const FarmRules& rules = {}) noexcept;
	void Rebuild(const FarmGrid& grid);
	bool UpdateWater(FarmGrid& grid, float deltaTime, float timeScale);
	[[nodiscard]] float GetAvailableIrrigationStrength(const FarmGrid& grid, int tileIndex) const noexcept;
	[[nodiscard]] FarmWaterStatus GetWaterStatus(const FarmGrid& grid, int tileIndex) const noexcept;
	[[nodiscard]] int GetAvailableCanalIndex(const FarmGrid& grid, int tileIndex) const noexcept {
		return FindWetCanal(grid, tileIndex);
	}

	[[nodiscard]] bool IsSupplied(int tileIndex) const noexcept;
	[[nodiscard]] bool IsInIrrigationRange(int tileIndex) const noexcept;
	[[nodiscard]] float GetSupplyStrength(int tileIndex) const noexcept;
	[[nodiscard]] float GetIrrigationStrength(int tileIndex) const noexcept;
	[[nodiscard]] int GetDownstreamCanalCount(int tileIndex) const noexcept;
	[[nodiscard]] int GetUpstreamTileIndex(int tileIndex) const noexcept;
	[[nodiscard]] int GetSupplyingCanalIndex(int tileIndex) const noexcept;
	[[nodiscard]] int GetWaterSourceCount() const noexcept { return waterSourceCount_; }
	[[nodiscard]] int GetSuppliedCanalCount() const noexcept { return suppliedCanalCount_; }
	[[nodiscard]] int GetIrrigationRangeTileCount() const noexcept {
		return irrigationRangeTileCount_;
	}

private:
	[[nodiscard]] int FindWetCanal(const FarmGrid& grid, int tileIndex) const noexcept;
	std::vector<float> waterBefore_, waterDelta_, soilDemand_, canalDemand_;
	std::vector<int> soilCanals_;
	float refillPerSecond_ = 0.5f;
	float transferPerSecond_ = 0.4f;
	float recoveryPerSecond_ = 0.125f;
	float maxDeltaTime_ = 0.25f;
	std::vector<std::uint8_t> supplied_;
	std::vector<std::uint8_t> irrigationRange_;
	std::vector<float> supplyStrengths_;
	std::vector<float> irrigationStrengths_;
	std::vector<int> downstreamCanalCounts_;
	std::vector<int> upstreamTileIndices_;
	std::vector<int> supplyingCanalIndices_;
	std::vector<int> traversalQueue_;
	int waterSourceCount_ = 0;
	int suppliedCanalCount_ = 0;
	int irrigationRangeTileCount_ = 0;
	float sourceStrength_ = 1.0f;
	float canalTransferEfficiency_ = 0.90f;
};

} // namespace farm
