#pragma once

#include "farm/core/FarmTypes.h"
#include "farm/data/FarmRules.h"
#include "farm/system/FarmCropQualitySystem.h"
#include "farm/system/FarmToolSystem.h"
#include "command/CommandHistory.h"

#include <vector>

namespace farm {
class FarmGrid;
struct FarmTile;
}

class FarmEconomySystem;

enum class FarmToolActionStatus {
	None,
	Applied,
	Harvested,
	InvalidTarget,
	InvalidState,
	AlreadyWatered,
	NoSeed,
	NotReady,
	UnsupportedTool,
};

struct FarmToolActionResult {
	FarmToolActionStatus status = FarmToolActionStatus::None;
	FarmTool tool = FarmTool::Hoe;
	int tileIndex = -1;
	int reward = 0;
	FarmCropQualityResult harvestQuality{};
	float moistureBefore = 0.0f;
	float moistureAfter = 0.0f;

	[[nodiscard]] bool Succeeded() const noexcept {
		return status == FarmToolActionStatus::Applied ||
			status == FarmToolActionStatus::Harvested;
	}
};

class FarmToolActionSystem {
public:
	static constexpr int kMinimumHeightLevel = 0;
	static constexpr int kMaximumHeightLevel = 2;

	void Initialize(const farm::FarmRules& rules = {}) noexcept;
	[[nodiscard]] FarmCropQualityResult EvaluateHarvestQuality(
		const farm::FarmTile& tile) const noexcept;

	[[nodiscard]] FarmToolActionResult EvaluateTool(
		const farm::FarmGrid& grid, FarmTool tool, farm::CropType selectedCrop,
		const FarmEconomySystem* economySystem = nullptr) const noexcept;
	[[nodiscard]] FarmToolActionResult EvaluateTool(
		const farm::FarmGrid& grid, int tileIndex, FarmTool tool,
		farm::CropType selectedCrop,
		const FarmEconomySystem* economySystem = nullptr) const noexcept;
	bool ApplyTool(
		farm::FarmGrid& grid, FarmTool tool, farm::CropType selectedCrop,
		FarmEconomySystem& economySystem);
	[[nodiscard]] FarmToolActionResult ApplyToolDetailed(
		farm::FarmGrid& grid, FarmTool tool, farm::CropType selectedCrop,
		FarmEconomySystem& economySystem);
	bool RaiseSelectedTile(farm::FarmGrid& grid);
	bool LowerSelectedTile(farm::FarmGrid& grid);
	[[nodiscard]] bool CanToggleCanal(
		const farm::FarmGrid& grid, int tileIndex) const noexcept;
	bool ToggleSelectedCanal(farm::FarmGrid& grid);
	bool PlaceCanalPath(farm::FarmGrid& grid, const std::vector<int>& tileIndices);
	bool RemoveCanalPath(farm::FarmGrid& grid, const std::vector<int>& tileIndices);
	[[nodiscard]] bool CanToggleWaterSource(
		const farm::FarmGrid& grid, int tileIndex) const noexcept;
	bool ToggleSelectedWaterSource(farm::FarmGrid& grid);
	bool Undo() { return history_.Undo(); }
	bool Redo() { return history_.Redo(); }
	void ClearHistory() noexcept { history_.Clear(); }
	[[nodiscard]] const CommandHistory& GetHistory() const noexcept { return history_; }

private:
	bool CommitCanalPath(farm::FarmGrid& grid, const std::vector<int>& tileIndices, bool remove);
	bool CommitTileChange(
		farm::FarmGrid& grid, int tileIndex, const farm::FarmTile& before,
		const farm::FarmTile& after, const char* commandName,
		FarmEconomySystem* economySystem = nullptr,
		const FarmCropQualityResult& harvestedQuality = {},
		int harvestedQuantity = 0,
		farm::CropType plantedCrop = farm::CropType::None,
		int plantedQuantity = 0);
	CommandHistory history_{ 128 };
	FarmCropQualitySystem cropQualitySystem_{};
	float wateringMoistureIncrement_ = farm::FarmRules{}.wateringMoistureIncrement;
};
