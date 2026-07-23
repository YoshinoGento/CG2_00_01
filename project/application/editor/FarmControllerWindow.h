#pragma once

#include "editor/EditorLocalization.h"
#include "editor/GamePlayEditorBridge.h"

#include <optional>

struct FarmControllerActions {
	std::optional<FarmTool> selectedTool;
	bool applyCurrentTool = false;
	bool raiseTile = false;
	bool lowerTile = false;
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
