#include "editor/EditorSettings.h"

#include "base/ImGuiManager.h"
#include "base/Logger.h"
#include "io/JsonFile.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui_internal.h"
#endif

#include <algorithm>
#include <filesystem>
#include <system_error>
#include <utility>

namespace {
constexpr const char* kEditorSettingsPath = "Settings/editor/editor_settings.json";
constexpr float kMinUiScale = 0.75f;
constexpr float kMaxUiScale = 1.50f;
constexpr int kMaxWorkspaceLayoutVersion = 1024;

EditorTheme ParseTheme(const std::string& value) {
	if (value == "Light") {
		return EditorTheme::Light;
	}
	if (value == "Classic") {
		return EditorTheme::Classic;
	}
	return EditorTheme::Dark;
}
} // namespace

void EditorSettings::Load() {
	nlohmann::json json;
	if (!JsonFile::Exists(kEditorSettingsPath) || !JsonFile::Load(kEditorSettingsPath, json)) {
		return;
	}

	if (json.contains("font") && json["font"].is_string()) {
		fontName_ = json["font"].get<std::string>();
	}
	if (json.contains("uiScale") && json["uiScale"].is_number()) {
		uiScale_ = std::clamp(json["uiScale"].get<float>(), kMinUiScale, kMaxUiScale);
	}
	if (json.contains("theme") && json["theme"].is_string()) {
		theme_ = ParseTheme(json["theme"].get<std::string>());
	}
	if (json.contains("language") && json["language"].is_string()) {
		language_ = editor::ParseEditorLanguage(json["language"].get<std::string>());
	}
	if (json.contains("workspaceLayoutVersion") && json["workspaceLayoutVersion"].is_number_integer()) {
		workspaceLayoutVersion_ = std::clamp(
			json["workspaceLayoutVersion"].get<int>(),
			0,
			kMaxWorkspaceLayoutVersion);
	}
}

bool EditorSettings::Save() const {
	std::error_code error;
	std::filesystem::create_directories(std::filesystem::path(kEditorSettingsPath).parent_path(), error);
	if (error) {
		Logger::Log("EditorSettings::Save failed to create directory: " + error.message());
		return false;
	}

	nlohmann::json json;
	json["version"] = 3;
	json["font"] = fontName_;
	json["uiScale"] = uiScale_;
	json["theme"] = GetThemeName(theme_);
	json["language"] = editor::GetEditorLanguageStorageName(language_);
	json["workspaceLayoutVersion"] = workspaceLayoutVersion_;
	return JsonFile::Save(kEditorSettingsPath, json);
}

bool EditorSettings::Apply(ImGuiManager& imguiManager) {
#ifdef USE_IMGUI
	if (ImGui::GetCurrentContext() == nullptr) {
		return false;
	}

	bool selectedConfiguredFont = false;
	for (std::size_t index = 0; index < imguiManager.GetFontOptionCount(); ++index) {
		if (fontName_ == imguiManager.GetFontOptionName(index)) {
			selectedConfiguredFont = imguiManager.SelectFont(index);
			break;
		}
	}
	if (!selectedConfiguredFont && imguiManager.GetFontOptionCount() > 0) {
		imguiManager.SelectFont(0);
		fontName_ = imguiManager.GetFontOptionName(0);
	}

	ImGuiStyle& style = ImGui::GetStyle();
	style = ImGuiStyle{};
	switch (theme_) {
	case EditorTheme::Light:
		ImGui::StyleColorsLight(&style);
		break;
	case EditorTheme::Classic:
		ImGui::StyleColorsClassic(&style);
		break;
	case EditorTheme::Dark:
	default:
		ImGui::StyleColorsDark(&style);
		break;
	}

	style.ScaleAllSizes(uiScale_);
	style.FontScaleMain = uiScale_;
	if ((ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) != 0) {
		style.WindowRounding = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}
	return true;
#else
	(void)imguiManager;
	return false;
#endif
}

bool EditorSettings::ResetLayout() {
#ifdef USE_IMGUI
	if (ImGui::GetCurrentContext() == nullptr) {
		return false;
	}
	ImGui::ClearIniSettings();
	return true;
#else
	return false;
#endif
}

const std::string& EditorSettings::GetFontName() const noexcept {
	return fontName_;
}

float EditorSettings::GetUiScale() const noexcept {
	return uiScale_;
}

EditorTheme EditorSettings::GetTheme() const noexcept {
	return theme_;
}

EditorLanguage EditorSettings::GetLanguage() const noexcept {
	return language_;
}

int EditorSettings::GetWorkspaceLayoutVersion() const noexcept {
	return workspaceLayoutVersion_;
}

bool EditorSettings::SetFontName(std::string fontName) {
	if (fontName.empty() || fontName_ == fontName) {
		return false;
	}
	fontName_ = std::move(fontName);
	return true;
}

bool EditorSettings::SetUiScale(float uiScale) {
	const float clampedScale = std::clamp(uiScale, kMinUiScale, kMaxUiScale);
	if (uiScale_ == clampedScale) {
		return false;
	}
	uiScale_ = clampedScale;
	return true;
}

bool EditorSettings::SetTheme(EditorTheme theme) {
	if (theme_ == theme) {
		return false;
	}
	theme_ = theme;
	return true;
}

bool EditorSettings::SetLanguage(EditorLanguage language) noexcept {
	if (language_ == language) {
		return false;
	}
	language_ = language;
	return true;
}

bool EditorSettings::SetWorkspaceLayoutVersion(int version) noexcept {
	const int clampedVersion = std::clamp(version, 0, kMaxWorkspaceLayoutVersion);
	if (workspaceLayoutVersion_ == clampedVersion) {
		return false;
	}
	workspaceLayoutVersion_ = clampedVersion;
	return true;
}

const char* EditorSettings::GetThemeName(EditorTheme theme) noexcept {
	switch (theme) {
	case EditorTheme::Light:
		return "Light";
	case EditorTheme::Classic:
		return "Classic";
	case EditorTheme::Dark:
	default:
		return "Dark";
	}
}

const char* EditorSettings::GetSettingsPath() noexcept {
	return kEditorSettingsPath;
}
