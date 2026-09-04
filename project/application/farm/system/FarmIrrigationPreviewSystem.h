#pragma once

#include "farm/core/FarmGrid.h"
#include "farm/data/FarmRules.h"
#include "farm/system/FarmIrrigationSystem.h"

#include <cstdint>

namespace farm {

enum class FarmIrrigationPreviewOperation : std::uint8_t {
	None,
	ToggleCanal,
	ToggleWaterSource,
	RaiseTerrain,
	LowerTerrain,
};

// Owns one temporary irrigation edit. Authoritative FarmGrid mutation stays elsewhere.
class FarmIrrigationPreviewSystem final {
public:
	void Initialize(const FarmRules& rules = {}) noexcept;
	[[nodiscard]] bool Begin(
		const FarmGrid& sourceGrid,
		int tileIndex,
		FarmIrrigationPreviewOperation operation);
	void Cancel() noexcept;

	[[nodiscard]] bool CanConfirm(const FarmGrid& sourceGrid) const noexcept;
	[[nodiscard]] bool IsActive() const noexcept { return active_; }
	[[nodiscard]] int GetTileIndex() const noexcept { return tileIndex_; }
	[[nodiscard]] FarmIrrigationPreviewOperation GetOperation() const noexcept {
		return operation_;
	}
	[[nodiscard]] FarmTileFeature GetOriginalFeature() const noexcept {
		return originalTile_.feature;
	}
	[[nodiscard]] FarmTileFeature GetCandidateFeature() const noexcept {
		return candidateFeature_;
	}
	[[nodiscard]] int GetOriginalHeightLevel() const noexcept {
		return originalTile_.heightLevel;
	}
	[[nodiscard]] int GetCandidateHeightLevel() const noexcept {
		return candidateHeightLevel_;
	}
	[[nodiscard]] const FarmGrid* GetPreviewGrid() const noexcept {
		return active_ ? &previewGrid_ : nullptr;
	}
	[[nodiscard]] const FarmIrrigationSystem* GetPreviewIrrigation() const noexcept {
		return active_ ? &previewIrrigation_ : nullptr;
	}

private:
	FarmRules rules_{};
	FarmGrid previewGrid_{};
	FarmIrrigationSystem previewIrrigation_{};
	FarmGrid::Snapshot sourceSnapshot_{};
	FarmTile originalTile_{};
	int tileIndex_ = -1;
	FarmIrrigationPreviewOperation operation_ = FarmIrrigationPreviewOperation::None;
	FarmTileFeature candidateFeature_ = FarmTileFeature::None;
	int candidateHeightLevel_ = 0;
	bool active_ = false;
};

} // namespace farm
