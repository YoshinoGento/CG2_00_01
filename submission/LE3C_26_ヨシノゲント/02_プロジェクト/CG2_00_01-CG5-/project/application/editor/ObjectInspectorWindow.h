#pragma once

#include "editor/GamePlayEditorBridge.h"

class ObjectInspectorWindow final {
public:
	[[nodiscard]] editor::ObjectInspectorEditorCommand Draw(
		const editor::ObjectInspectorEditorViewData& viewData);
	void SetOpen(bool open) noexcept;
	[[nodiscard]] bool IsOpen() const noexcept;

private:
	bool open_ = true;
	int selectedJointIndex_ = 0;
};
