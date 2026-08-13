#pragma once

#include "farm/core/FarmTypes.h"
#include "farm/system/FarmToolSystem.h"
#include "command/CommandHistory.h"

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
	NotReady,
	UnsupportedTool,
};

struct FarmToolActionResult {
	FarmToolActionStatus status = FarmToolActionStatus::None;
	FarmTool tool = FarmTool::Hoe;
	int tileIndex = -1;
	int reward = 0;

	[[nodiscard]] bool Succeeded() const noexcept {
		return status == FarmToolActionStatus::Applied ||
			status == FarmToolActionStatus::Harvested;
	}
};

class FarmToolActionSystem {
public:
	static constexpr int kMinimumHeightLevel = 0;
	static constexpr int kMaximumHeightLevel = 2;

	[[nodiscard]] FarmToolActionResult EvaluateTool(
		const farm::FarmGrid& grid, FarmTool tool) const noexcept;
	[[nodiscard]] FarmToolActionResult EvaluateTool(
		const farm::FarmGrid& grid, int tileIndex, FarmTool tool) const noexcept;
	bool ApplyTool(farm::FarmGrid& grid, FarmTool tool);
	[[nodiscard]] FarmToolActionResult ApplyToolDetailed(
		farm::FarmGrid& grid, FarmTool tool, FarmEconomySystem* economySystem = nullptr);
	bool RaiseSelectedTile(farm::FarmGrid& grid);
	bool LowerSelectedTile(farm::FarmGrid& grid);
	bool Undo() { return history_.Undo(); }
	bool Redo() { return history_.Redo(); }
	void ClearHistory() noexcept { history_.Clear(); }
	[[nodiscard]] const CommandHistory& GetHistory() const noexcept { return history_; }

private:
	bool CommitTileChange(
		farm::FarmGrid& grid, int tileIndex, const farm::FarmTile& before,
		const farm::FarmTile& after, const char* commandName,
		FarmEconomySystem* economySystem = nullptr,
		farm::CropType harvestedCrop = farm::CropType::None,
		int harvestedQuantity = 0);
	CommandHistory history_{ 128 };
};
