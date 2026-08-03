#pragma once

#include "editor/EditorLocalization.h"
#include "editor/GamePlayEditorBridge.h"

struct FarmHistoryActions {
	bool undo = false;
	bool redo = false;
};

// Presents the scene-owned CommandHistory without owning or mutating it.
class FarmHistoryWindow final {
public:
	[[nodiscard]] FarmHistoryActions Draw(
		const editor::GamePlayEditorViewModel& viewModel,
		EditorLanguage language);

	void SetOpen(bool open) noexcept { open_ = open; }
	[[nodiscard]] bool IsOpen() const noexcept { return open_; }

private:
	bool open_ = true;
};
