#include "farm/system/FarmIrrigationSystem.h"

#include "farm/core/FarmGrid.h"

#include <algorithm>
#include <array>
#include <cstddef>

namespace farm {
namespace {
constexpr std::array<int, 4> kNeighborX = { 0, 0, -1, 1 };
constexpr std::array<int, 4> kNeighborY = { -1, 1, 0, 0 };
}

void FarmIrrigationSystem::Initialize() noexcept
{
	supplied_.clear();
	traversalQueue_.clear();
	waterSourceCount_ = 0;
	suppliedCanalCount_ = 0;
}

void FarmIrrigationSystem::Rebuild(const FarmGrid& grid)
{
	const int tileCount = grid.GetTileCount();
	waterSourceCount_ = 0;
	suppliedCanalCount_ = 0;
	if (tileCount <= 0 || grid.GetWidth() <= 0 || grid.GetHeight() <= 0) {
		supplied_.clear();
		traversalQueue_.clear();
		return;
	}

	const std::size_t requiredSize = static_cast<std::size_t>(tileCount);
	if (supplied_.size() != requiredSize) {
		supplied_.resize(requiredSize);
	}
	std::fill(supplied_.begin(), supplied_.end(), std::uint8_t{ 0 });
	traversalQueue_.clear();
	if (traversalQueue_.capacity() < requiredSize) {
		traversalQueue_.reserve(requiredSize);
	}

	for (int index = 0; index < tileCount; ++index) {
		const FarmTile* tile = grid.GetTile(index);
		if (tile != nullptr && tile->feature == FarmTileFeature::WaterSource) {
			supplied_[static_cast<std::size_t>(index)] = 1;
			traversalQueue_.push_back(index);
			++waterSourceCount_;
		}
	}

	std::size_t queueHead = 0;
	while (queueHead < traversalQueue_.size()) {
		const int currentIndex = traversalQueue_[queueHead++];
		const FarmTile* currentTile = grid.GetTile(currentIndex);
		if (currentTile == nullptr) {
			continue;
		}
		const int currentX = currentIndex % grid.GetWidth();
		const int currentY = currentIndex / grid.GetWidth();
		for (std::size_t direction = 0; direction < kNeighborX.size(); ++direction) {
			const int nextX = currentX + kNeighborX[direction];
			const int nextY = currentY + kNeighborY[direction];
			if (nextX < 0 || nextX >= grid.GetWidth() ||
				nextY < 0 || nextY >= grid.GetHeight()) {
				continue;
			}
			const int nextIndex = nextY * grid.GetWidth() + nextX;
			if (supplied_[static_cast<std::size_t>(nextIndex)] != 0) {
				continue;
			}
			const FarmTile* nextTile = grid.GetTile(nextIndex);
			if (nextTile == nullptr || nextTile->feature != FarmTileFeature::Canal ||
				nextTile->heightLevel > currentTile->heightLevel) {
				continue;
			}
			supplied_[static_cast<std::size_t>(nextIndex)] = 1;
			traversalQueue_.push_back(nextIndex);
			++suppliedCanalCount_;
		}
	}
}

bool FarmIrrigationSystem::IsSupplied(int tileIndex) const noexcept
{
	return tileIndex >= 0 &&
		static_cast<std::size_t>(tileIndex) < supplied_.size() &&
		supplied_[static_cast<std::size_t>(tileIndex)] != 0;
}

} // namespace farm
