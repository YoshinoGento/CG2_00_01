#pragma once

#include "editor/GamePlayEditorBridge.h"

class ParticleEffectWindow final {
public:
	[[nodiscard]] editor::ParticleEditorCommand Draw(const editor::ParticleEditorViewData& viewData);
	void SetOpen(bool open) noexcept;
	[[nodiscard]] bool IsOpen() const noexcept;

private:
	bool open_ = true;
};
