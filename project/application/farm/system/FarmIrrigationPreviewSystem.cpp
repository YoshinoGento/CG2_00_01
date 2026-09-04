#include "farm/system/FarmIrrigationPreviewSystem.h"

#include "farm/system/FarmToolActionSystem.h"

#include <cstddef>
#include <cmath>

namespace farm {
namespace {
bool TilesEqual(const FarmTile& left, const FarmTile& right) noexcept
{
	return left.heightLevel == right.heightLevel &&
		left.feature == right.feature &&
		left.state == right.state &&
		left.crop == right.crop &&
		left.moisture == right.moisture &&
		left.growth == right.growth;
}

bool IsUnusedTile(const FarmTile& tile) noexcept
{
	return tile.state == FarmTileState::Empty &&
		tile.crop == CropType::None &&
		std::isfinite(tile.moisture) && tile.moisture == 0.0f &&
		std::isfinite(tile.growth) && tile.growth == 0.0f;
}

bool TryBuildCandidateTile(
	const FarmTile& source,
	FarmIrrigationPreviewOperation operation,
	FarmTile& output) noexcept
{
	output = source;
	switch (operation) {
	case FarmIrrigationPreviewOperation::ToggleCanal:
		if (source.feature == FarmTileFeature::Canal) {
			output.feature = FarmTileFeature::None;
			return true;
		}
		if (source.feature == FarmTileFeature::None && IsUnusedTile(source)) {
			output.feature = FarmTileFeature::Canal;
			return true;
		}
		return false;
	case FarmIrrigationPreviewOperation::ToggleWaterSource:
		if (source.feature == FarmTileFeature::WaterSource) {
			output.feature = FarmTileFeature::None;
			return true;
		}
		if (source.feature == FarmTileFeature::None && IsUnusedTile(source)) {
			output.feature = FarmTileFeature::WaterSource;
			return true;
		}
		return false;
	case FarmIrrigationPreviewOperation::RaiseTerrain:
		if (source.heightLevel >= FarmToolActionSystem::kMaximumHeightLevel) {
			return false;
		}
		++output.heightLevel;
		return true;
	case FarmIrrigationPreviewOperation::LowerTerrain:
		if (source.heightLevel <= FarmToolActionSystem::kMinimumHeightLevel) {
			return false;
		}
		--output.heightLevel;
		return true;
	case FarmIrrigationPreviewOperation::None:
	default:
		return false;
	}
}
} // namespace

void FarmIrrigationPreviewSystem::Initialize(const FarmRules& rules) noexcept
{
	rules_ = rules;
	Cancel();
}

bool FarmIrrigationPreviewSystem::Begin(
	const FarmGrid& sourceGrid,
	int tileIndex,
	FarmIrrigationPreviewOperation operation)
{
	Cancel();
	const FarmTile* sourceTile = sourceGrid.GetTile(tileIndex);
	if (sourceTile == nullptr || !IsValidFarmTileFeature(sourceTile->feature)) {
		return false;
	}

	FarmTile candidateTile{};
	if (!TryBuildCandidateTile(*sourceTile, operation, candidateTile)) {
		return false;
	}

	previewGrid_ = sourceGrid;
	if (!previewGrid_.SetTile(tileIndex, candidateTile) ||
		!previewGrid_.SetSelectedIndex(tileIndex)) {
		Cancel();
		return false;
	}

	previewIrrigation_.Initialize(rules_);
	previewIrrigation_.Rebuild(previewGrid_);
	sourceGrid.CaptureSnapshot(sourceSnapshot_);
	originalTile_ = *sourceTile;
	tileIndex_ = tileIndex;
	operation_ = operation;
	candidateFeature_ = candidateTile.feature;
	candidateHeightLevel_ = candidateTile.heightLevel;
	active_ = true;
	return true;
}

void FarmIrrigationPreviewSystem::Cancel() noexcept
{
	previewGrid_ = {};
	previewIrrigation_.Initialize(rules_);
	sourceSnapshot_ = {};
	originalTile_ = {};
	tileIndex_ = -1;
	operation_ = FarmIrrigationPreviewOperation::None;
	candidateFeature_ = FarmTileFeature::None;
	candidateHeightLevel_ = 0;
	active_ = false;
}

bool FarmIrrigationPreviewSystem::CanConfirm(const FarmGrid& sourceGrid) const noexcept
{
	if (!active_) {
		return false;
	}
	if (sourceGrid.GetWidth() != sourceSnapshot_.width ||
		sourceGrid.GetHeight() != sourceSnapshot_.height ||
		sourceGrid.GetTileCount() != static_cast<int>(sourceSnapshot_.tiles.size())) {
		return false;
	}
	for (int index = 0; index < sourceGrid.GetTileCount(); ++index) {
		const FarmTile* currentTile = sourceGrid.GetTile(index);
		if (currentTile == nullptr ||
			!TilesEqual(*currentTile, sourceSnapshot_.tiles[static_cast<std::size_t>(index)])) {
			return false;
		}
	}
	return true;
}

} // namespace farm
