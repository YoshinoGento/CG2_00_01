#pragma once

#include "editor/EditorLocalization.h"
#include "math/Matrix.h"

#include <cstdint>

class SrvManager;

struct GameViewportFrameState {
	Vector2 imageTopLeft{};
	Vector2 imageSize{};
	Vector2 mousePosition{};
	Vector2 virtualMousePosition{};
	bool hovered = false;
	bool focused = false;
	bool leftClicked = false;
	bool quickApplyRequested = false;
	bool imageVisible = false;
};

// Displays FinalDisplayTexture and reports the exact image rectangle used by input systems.
class GameViewportWindow final {
public:
	void Draw(
		SrvManager& srvManager,
		uint32_t finalDisplaySrvIndex,
		const Vector2& virtualResolution,
		EditorLanguage language);

	const GameViewportFrameState& GetFrameState() const { return frameState_; }

private:
	GameViewportFrameState frameState_{};
};
