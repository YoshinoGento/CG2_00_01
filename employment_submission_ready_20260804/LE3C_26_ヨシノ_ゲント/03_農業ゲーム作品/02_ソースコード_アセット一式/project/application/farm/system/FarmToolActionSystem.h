#pragma once

#include "farm/system/FarmToolSystem.h"
#include "command/CommandHistory.h"

namespace farm {
class FarmGrid;
struct FarmTile;
}

class FarmToolActionSystem {
public:
	bool ApplyTool(farm::FarmGrid& grid, FarmTool tool);
	bool RaiseSelectedTile(farm::FarmGrid& grid);
	bool LowerSelectedTile(farm::FarmGrid& grid);
	bool Undo() { return history_.Undo(); }
	bool Redo() { return history_.Redo(); }
	void ClearHistory() noexcept { history_.Clear(); }
	[[nodiscard]] const CommandHistory& GetHistory() const noexcept { return history_; }

private:
	bool CommitTileChange(
		farm::FarmGrid& grid, int tileIndex, const farm::FarmTile& before,
		const farm::FarmTile& after, const char* commandName);
	CommandHistory history_{ 128 };
};
