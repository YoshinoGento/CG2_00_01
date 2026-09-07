#pragma once

#include "editor/EditorLocalization.h"
#include "editor/GamePlayEditorBridge.h"

#include <optional>

struct FarmControllerActions {
	std::optional<FarmTool> selectedTool;
	bool applyCurrentTool = false;
	bool beginRaiseTerrainPreview = false;
	bool beginLowerTerrainPreview = false;
	bool toggleCanal = false;
	bool toggleWaterSource = false;
	bool beginCanalPreview = false;
	bool beginWaterSourcePreview = false;
	bool confirmIrrigationPreview = false;
	bool cancelIrrigationPreview = false;
	bool restartFarm = false;
	bool movePlayerToSelectedTile = false;
	std::optional<editor::GamePlayEditorCommandType> comparisonCommand;
};

// Compact viewport companion. It reports actions and never mutates Farm state directly.
class FarmControllerWindow final {
public:
	[[nodiscard]] FarmControllerActions Draw(
		const editor::GamePlayEditorViewModel& viewModel,
		EditorLanguage language);

	void SetOpen(bool open) noexcept { open_ = open; }
	[[nodiscard]] bool IsOpen() const noexcept { return open_; }

private:
	bool open_ = true;
};
