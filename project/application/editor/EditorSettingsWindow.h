#pragma once

#include "editor/EditorSettings.h"

#include <cstddef>
#include <optional>

class ImGuiManager;

struct EditorSettingsActions {
	std::optional<std::size_t> fontIndex;
	std::optional<float> uiScale;
	std::optional<EditorTheme> theme;
	std::optional<EditorLanguage> language;
	bool resetLayout = false;
};

class EditorSettingsWindow final {
public:
	[[nodiscard]] EditorSettingsActions Draw(
		const EditorSettings& settings,
		const ImGuiManager& imguiManager);

	void SetOpen(bool open) noexcept;
	[[nodiscard]] bool IsOpen() const noexcept;

private:
	bool open_ = false;
};
