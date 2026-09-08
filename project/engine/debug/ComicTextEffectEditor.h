#pragma once

#ifdef USE_IMGUI

#include "effect/ComicTextEffect.h"

#include <array>
#include <string>
#include <vector>

class ComicTextEffectEditor final {
public:
	ComicTextEffectEditor();
	void Draw(ComicTextEffectSystem& system, const Vector3& previewPosition,
		bool previewRequested = false);

private:
	void RefreshPresetList();
	void SelectPreset(int index);
	void DrawPresetLibrary();
	bool LoadCurrentPreset();
	bool SaveCurrentPreset(bool overwrite);
	void RenameSelectedPreset();
	void DeleteSelectedPreset();

	ComicTextEffectPreset preset_{};
	std::array<char, 128> presetName_{};
	std::array<char, 256> texturePath_{};
	std::array<char, 256> text_{};
	std::string status_;
	std::vector<std::string> presetNames_;
	int selectedPresetIndex_ = -1;
	std::string pendingDeletePreset_;
};

#endif
