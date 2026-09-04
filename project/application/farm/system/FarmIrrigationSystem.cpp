#include "farm/system/FarmIrrigationSystem.h"

#include "farm/core/FarmGrid.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cmath>

namespace farm {
namespace {
constexpr std::array<int, 4> kNeighborX = { 0, 0, -1, 1 };
constexpr std::array<int, 4> kNeighborY = { -1, 1, 0, 0 };

float SanitizeStrength(float value, float fallback) noexcept
{
	return std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : fallback;
}
}

void FarmIrrigationSystem::Initialize(const FarmRules& rules) noexcept
{
	const FarmRules defaults{};
	supplied_.clear();
	irrigationRange_.clear();
	supplyStrengths_.clear();
	irrigationStrengths_.clear();
	downstreamCanalCounts_.clear();
	upstreamTileIndices_.clear();
	supplyingCanalIndices_.clear();
	traversalQueue_.clear();
	waterSourceCount_ = 0;
	suppliedCanalCount_ = 0;
	irrigationRangeTileCount_ = 0;
	sourceStrength_ = SanitizeStrength(
		rules.irrigationSourceStrength, defaults.irrigationSourceStrength);
	canalTransferEfficiency_ = SanitizeStrength(
		rules.irrigationCanalTransferEfficiency,
		defaults.irrigationCanalTransferEfficiency);
}

void FarmIrrigationSystem::Rebuild(const FarmGrid& grid)
{
	const int tileCount = grid.GetTileCount();
	waterSourceCount_ = 0;
	suppliedCanalCount_ = 0;
	irrigationRangeTileCount_ = 0;
	if (tileCount <= 0 || grid.GetWidth() <= 0 || grid.GetHeight() <= 0) {
		supplied_.clear();
		irrigationRange_.clear();
		supplyStrengths_.clear();
		irrigationStrengths_.clear();
		downstreamCanalCounts_.clear();
		upstreamTileIndices_.clear();
		supplyingCanalIndices_.clear();
		traversalQueue_.clear();
		return;
	}

	const std::size_t requiredSize = static_cast<std::size_t>(tileCount);
	if (supplied_.size() != requiredSize) {
		supplied_.resize(requiredSize);
	}
	if (irrigationRange_.size() != requiredSize) {
		irrigationRange_.resize(requiredSize);
	}
	if (supplyStrengths_.size() != requiredSize) {
		supplyStrengths_.resize(requiredSize);
	}
	if (irrigationStrengths_.size() != requiredSize) {
		irrigationStrengths_.resize(requiredSize);
	}
	if (downstreamCanalCounts_.size() != requiredSize) {
		downstreamCanalCounts_.resize(requiredSize);
	}
	if (upstreamTileIndices_.size() != requiredSize) {
		upstreamTileIndices_.resize(requiredSize);
	}
	if (supplyingCanalIndices_.size() != requiredSize) {
		supplyingCanalIndices_.resize(requiredSize);
	}
	std::fill(supplied_.begin(), supplied_.end(), std::uint8_t{ 0 });
	std::fill(irrigationRange_.begin(), irrigationRange_.end(), std::uint8_t{ 0 });
	std::fill(supplyStrengths_.begin(), supplyStrengths_.end(), 0.0f);
	std::fill(irrigationStrengths_.begin(), irrigationStrengths_.end(), 0.0f);
	std::fill(downstreamCanalCounts_.begin(), downstreamCanalCounts_.end(), 0);
	std::fill(upstreamTileIndices_.begin(), upstreamTileIndices_.end(), -1);
	std::fill(supplyingCanalIndices_.begin(), supplyingCanalIndices_.end(), -1);
	traversalQueue_.clear();
	if (traversalQueue_.capacity() < requiredSize) {
		traversalQueue_.reserve(requiredSize);
	}

	for (int index = 0; index < tileCount; ++index) {
		const FarmTile* tile = grid.GetTile(index);
		if (tile != nullptr && tile->feature == FarmTileFeature::WaterSource) {
			supplied_[static_cast<std::size_t>(index)] = 1;
			supplyStrengths_[static_cast<std::size_t>(index)] = sourceStrength_;
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
			upstreamTileIndices_[static_cast<std::size_t>(nextIndex)] = currentIndex;
			traversalQueue_.push_back(nextIndex);
			++suppliedCanalCount_;
		}
	}

	for (int index : traversalQueue_) {
		const int upstreamIndex = GetUpstreamTileIndex(index);
		if (upstreamIndex >= 0 && upstreamIndex < tileCount) {
			++downstreamCanalCounts_[static_cast<std::size_t>(upstreamIndex)];
		}
	}
	for (int index : traversalQueue_) {
		const int upstreamIndex = GetUpstreamTileIndex(index);
		if (upstreamIndex < 0 || upstreamIndex >= tileCount) {
			continue;
		}
		const int branchCount = (std::max)(
			downstreamCanalCounts_[static_cast<std::size_t>(upstreamIndex)], 1);
		const float parentStrength =
			supplyStrengths_[static_cast<std::size_t>(upstreamIndex)];
		supplyStrengths_[static_cast<std::size_t>(index)] = std::clamp(
			parentStrength * canalTransferEfficiency_ /
				static_cast<float>(branchCount),
			0.0f,
			1.0f);
	}

	for (int index = 0; index < tileCount; ++index) {
		const FarmTile* tile = grid.GetTile(index);
		if (tile == nullptr || tile->feature != FarmTileFeature::None) {
			continue;
		}

		const int tileX = index % grid.GetWidth();
		const int tileY = index / grid.GetWidth();
		float strongestSupply = 0.0f;
		int strongestCanalIndex = -1;
		for (std::size_t direction = 0; direction < kNeighborX.size(); ++direction) {
			const int neighborX = tileX + kNeighborX[direction];
			const int neighborY = tileY + kNeighborY[direction];
			if (neighborX < 0 || neighborX >= grid.GetWidth() ||
				neighborY < 0 || neighborY >= grid.GetHeight()) {
				continue;
			}

			const int neighborIndex = neighborY * grid.GetWidth() + neighborX;
			const FarmTile* neighbor = grid.GetTile(neighborIndex);
			if (neighbor == nullptr || neighbor->feature != FarmTileFeature::Canal ||
				!IsSupplied(neighborIndex) || tile->heightLevel > neighbor->heightLevel) {
				continue;
			}

			const float candidateStrength = GetSupplyStrength(neighborIndex);
			if (candidateStrength > strongestSupply) {
				strongestSupply = candidateStrength;
				strongestCanalIndex = neighborIndex;
			}
		}
		if (strongestCanalIndex >= 0) {
			irrigationRange_[static_cast<std::size_t>(index)] = 1;
			irrigationStrengths_[static_cast<std::size_t>(index)] = strongestSupply;
			supplyingCanalIndices_[static_cast<std::size_t>(index)] = strongestCanalIndex;
			++irrigationRangeTileCount_;
		}
	}
}

bool FarmIrrigationSystem::IsSupplied(int tileIndex) const noexcept
{
	return tileIndex >= 0 &&
		static_cast<std::size_t>(tileIndex) < supplied_.size() &&
		supplied_[static_cast<std::size_t>(tileIndex)] != 0;
}

bool FarmIrrigationSystem::IsInIrrigationRange(int tileIndex) const noexcept
{
	return tileIndex >= 0 &&
		static_cast<std::size_t>(tileIndex) < irrigationRange_.size() &&
		irrigationRange_[static_cast<std::size_t>(tileIndex)] != 0;
}

float FarmIrrigationSystem::GetSupplyStrength(int tileIndex) const noexcept
{
	return tileIndex >= 0 &&
		static_cast<std::size_t>(tileIndex) < supplyStrengths_.size()
		? supplyStrengths_[static_cast<std::size_t>(tileIndex)]
		: 0.0f;
}

float FarmIrrigationSystem::GetIrrigationStrength(int tileIndex) const noexcept
{
	return tileIndex >= 0 &&
		static_cast<std::size_t>(tileIndex) < irrigationStrengths_.size()
		? irrigationStrengths_[static_cast<std::size_t>(tileIndex)]
		: 0.0f;
}

int FarmIrrigationSystem::GetDownstreamCanalCount(int tileIndex) const noexcept
{
	return tileIndex >= 0 &&
		static_cast<std::size_t>(tileIndex) < downstreamCanalCounts_.size()
		? downstreamCanalCounts_[static_cast<std::size_t>(tileIndex)]
		: 0;
}

int FarmIrrigationSystem::GetUpstreamTileIndex(int tileIndex) const noexcept
{
	return tileIndex >= 0 &&
		static_cast<std::size_t>(tileIndex) < upstreamTileIndices_.size()
		? upstreamTileIndices_[static_cast<std::size_t>(tileIndex)]
		: -1;
}

int FarmIrrigationSystem::GetSupplyingCanalIndex(int tileIndex) const noexcept
{
	return tileIndex >= 0 &&
		static_cast<std::size_t>(tileIndex) < supplyingCanalIndices_.size()
		? supplyingCanalIndices_[static_cast<std::size_t>(tileIndex)]
		: -1;
}

} // namespace farm
