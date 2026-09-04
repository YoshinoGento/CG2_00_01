#pragma once

#include "farm/data/FarmRules.h"

#include <cstdint>
#include <vector>

namespace farm {

class FarmGrid;

// Derives transient irrigation reachability from persistent terrain features.
class FarmIrrigationSystem final {
public:
	void Initialize(const FarmRules& rules = {}) noexcept;
	void Rebuild(const FarmGrid& grid);

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
