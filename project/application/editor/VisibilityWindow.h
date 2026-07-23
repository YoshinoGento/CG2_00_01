#pragma once

#include "editor/EditorLocalization.h"
#include "editor/GamePlayEditorBridge.h"

class VisibilityWindow final {
public:
	[[nodiscard]] editor::VisibilityEditorCommand Draw(
		const editor::VisibilityEditorViewData& viewData,
		EditorLanguage language);
	void SetOpen(bool open) noexcept;
	[[nodiscard]] bool IsOpen() const noexcept;

private:
	bool open_ = true;
};
