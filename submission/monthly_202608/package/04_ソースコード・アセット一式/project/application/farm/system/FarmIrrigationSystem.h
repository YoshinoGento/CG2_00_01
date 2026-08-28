#pragma once

#include <cstdint>
#include <vector>

namespace farm {

class FarmGrid;

// Derives transient irrigation reachability from persistent terrain features.
class FarmIrrigationSystem final {
public:
	void Initialize() noexcept;
	void Rebuild(const FarmGrid& grid);

	[[nodiscard]] bool IsSupplied(int tileIndex) const noexcept;
	[[nodiscard]] int GetWaterSourceCount() const noexcept { return waterSourceCount_; }
	[[nodiscard]] int GetSuppliedCanalCount() const noexcept { return suppliedCanalCount_; }

private:
	std::vector<std::uint8_t> supplied_;
	std::vector<int> traversalQueue_;
	int waterSourceCount_ = 0;
	int suppliedCanalCount_ = 0;
};

} // namespace farm
