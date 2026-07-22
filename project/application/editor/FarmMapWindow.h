#pragma once

#include "editor/EditorLocalization.h"
#include "editor/EditorSelectionSystem.h"
#include "editor/GamePlayEditorBridge.h"

#include <optional>

struct FarmMapActions {
	std::optional<int> selectedTileIndex;
};

// Spatial, read-only Farm selection view. Mutations stay in Farm systems.
class FarmMapWindow final {
public:
	[[nodiscard]] FarmMapActions Draw(
		const editor::GamePlayEditorViewModel& viewModel,
		const editor::EditorSelection& selection,
		EditorLanguage language);

	void SetOpen(bool open) noexcept { open_ = open; }
	[[nodiscard]] bool IsOpen() const noexcept { return open_; }

private:
	bool open_ = true;
};
