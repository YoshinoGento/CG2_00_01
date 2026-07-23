#pragma once

#include "editor/EditorLocalization.h"
#include "editor/EditorSelectionSystem.h"
#include "editor/GamePlayEditorBridge.h"

#include <optional>

struct FarmHierarchyActions {
	std::optional<int> selectedTileIndex;
};

// Read-only Farm tree. Selection changes are returned to the Editor command boundary.
class FarmHierarchyWindow final {
public:
	[[nodiscard]] FarmHierarchyActions Draw(
		const editor::GamePlayEditorViewModel& viewModel,
		const editor::EditorSelection& selection,
		EditorLanguage language);

	void SetOpen(bool open) noexcept { open_ = open; }
	[[nodiscard]] bool IsOpen() const noexcept { return open_; }

private:
	bool open_ = true;
};
