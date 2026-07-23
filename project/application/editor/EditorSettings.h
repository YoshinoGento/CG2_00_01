#pragma once

#include "editor/EditorLocalization.h"

#include <string>

class ImGuiManager;

enum class EditorTheme {
	Dark,
	Light,
	Classic,
};

// Persistent editor appearance settings. This class does not own any gameplay state.
class EditorSettings final {
public:
	void Load();
	bool Save() const;
	bool Apply(ImGuiManager& imguiManager);
	bool ResetLayout();

	[[nodiscard]] const std::string& GetFontName() const noexcept;
	[[nodiscard]] float GetUiScale() const noexcept;
	[[nodiscard]] EditorTheme GetTheme() const noexcept;
	[[nodiscard]] EditorLanguage GetLanguage() const noexcept;
	[[nodiscard]] int GetWorkspaceLayoutVersion() const noexcept;

	bool SetFontName(std::string fontName);
	bool SetUiScale(float uiScale);
	bool SetTheme(EditorTheme theme);
	bool SetLanguage(EditorLanguage language) noexcept;
	bool SetWorkspaceLayoutVersion(int version) noexcept;

	[[nodiscard]] static const char* GetThemeName(EditorTheme theme) noexcept;
	[[nodiscard]] static const char* GetSettingsPath() noexcept;

private:
	std::string fontName_ = "Noto Sans JP";
	float uiScale_ = 1.0f;
	EditorTheme theme_ = EditorTheme::Dark;
	EditorLanguage language_ = EditorLanguage::Japanese;
	int workspaceLayoutVersion_ = 0;
};
