#pragma once

#ifdef USE_IMGUI

#include "effect/ComicTextEffect.h"

#include <array>
#include <string>

class ComicTextEffectEditor final {
public:
	ComicTextEffectEditor();
	void Draw(ComicTextEffectSystem& system, const Vector3& previewPosition);

private:
	ComicTextEffectPreset preset_{};
	std::array<char, 64> presetName_{};
	std::array<char, 256> texturePath_{};
	std::array<char, 256> text_{};
	std::string status_;
};

#endif
