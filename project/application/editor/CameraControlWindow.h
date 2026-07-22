#pragma once

#include "editor/GamePlayEditorBridge.h"

class CameraControlWindow final {
public:
	[[nodiscard]] editor::CameraEditorCommand Draw(const editor::CameraEditorViewData& viewData);
	void SetOpen(bool open) noexcept;
	[[nodiscard]] bool IsOpen() const noexcept;

private:
	bool open_ = true;
};
