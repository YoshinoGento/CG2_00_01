#include "editor/EditorSettingsWindow.h"

#include "base/ImGuiManager.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

EditorSettingsActions EditorSettingsWindow::Draw(
	const EditorSettings& settings,
	const ImGuiManager& imguiManager) {
	EditorSettingsActions actions;
#ifdef USE_IMGUI
	if (!open_) {
		return actions;
	}
	const EditorLanguage language = settings.GetLanguage();
	const auto text = [language](std::string_view english) {
		return editor::Localize(language, english);
	};

	if (!ImGui::Begin(text("Editor Settings###EditorSettings"), &open_)) {
		ImGui::End();
		return actions;
	}

	ImGui::TextUnformatted(text("Editor Appearance"));
	ImGui::Separator();
	int languageIndex = static_cast<int>(language);
	const char* languageNames[] = {
		editor::Localize(EditorLanguage::Japanese, "Japanese"),
		editor::Localize(EditorLanguage::English, "English"),
	};
	if (ImGui::Combo(text("Language"), &languageIndex, languageNames, 2)) {
		actions.language = static_cast<EditorLanguage>(languageIndex);
	}

	const std::size_t selectedFontIndex = imguiManager.GetSelectedFontIndex();
	const char* selectedFontName = imguiManager.GetFontOptionName(selectedFontIndex);
	if (ImGui::BeginCombo(text("Font"), selectedFontName)) {
		for (std::size_t index = 0; index < imguiManager.GetFontOptionCount(); ++index) {
			const bool selected = index == selectedFontIndex;
			if (ImGui::Selectable(imguiManager.GetFontOptionName(index), selected)) {
				actions.fontIndex = index;
			}
			if (selected) {
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}

	float uiScale = settings.GetUiScale();
	if (ImGui::SliderFloat(text("UI Scale"), &uiScale, 0.75f, 1.50f, "%.2f")) {
		actions.uiScale = uiScale;
	}

	int themeIndex = static_cast<int>(settings.GetTheme());
	constexpr const char* kThemeNames[] = { "Dark", "Light", "Classic" };
	if (ImGui::Combo(text("Theme"), &themeIndex, kThemeNames, 3)) {
		actions.theme = static_cast<EditorTheme>(themeIndex);
	}

	ImGui::Spacing();
	if (ImGui::Button(text("Reset Window Layout"))) {
		actions.resetLayout = true;
	}
	ImGui::TextDisabled(text("Saved to %s"), EditorSettings::GetSettingsPath());
	ImGui::End();
#else
	(void)settings;
	(void)imguiManager;
#endif
	return actions;
}

void EditorSettingsWindow::SetOpen(bool open) noexcept {
	open_ = open;
}

bool EditorSettingsWindow::IsOpen() const noexcept {
	return open_;
}
