#include "farm/debug/FarmDebugEditorWindow.h"

#include "base/Logger.h"
#include "farm/core/FarmGrid.h"
#include "farm/system/FarmToolActionSystem.h"
#include "io/JsonFile.h"

#ifdef USE_IMGUI
#include "base/ImGuiManager.h"
#endif

#include <filesystem>
#include <system_error>

namespace farm {

namespace {
constexpr const char* kSettingsPath = "Settings/editor/farm_debug_editor_settings.json";
constexpr const char* kFarmEditorWindowKey = "farmEditorWindow";
}

void FarmDebugEditorWindow::LoadSettings()
{
#ifdef USE_IMGUI
	nlohmann::json json;
	if (!JsonFile::Exists(kSettingsPath)) {
		return;
	}
	if (!JsonFile::Load(kSettingsPath, json)) {
		return;
	}

	if (json.contains("showFarmDebugWindow") && json["showFarmDebugWindow"].is_boolean()) {
		showFarmDebugWindow_ = json["showFarmDebugWindow"].get<bool>();
	}

	if (json.contains(kFarmEditorWindowKey) && json[kFarmEditorWindowKey].is_object()) {
		const nlohmann::json& windowJson = json[kFarmEditorWindowKey];
		if (windowJson.contains("x") && windowJson["x"].is_number()) {
			farmEditorWindowX_ = windowJson["x"].get<float>();
		}
		if (windowJson.contains("y") && windowJson["y"].is_number()) {
			farmEditorWindowY_ = windowJson["y"].get<float>();
		}
		if (windowJson.contains("width") && windowJson["width"].is_number()) {
			farmEditorWindowWidth_ = windowJson["width"].get<float>();
		}
		if (windowJson.contains("height") && windowJson["height"].is_number()) {
			farmEditorWindowHeight_ = windowJson["height"].get<float>();
		}
		applyWindowPlacement_ = farmEditorWindowWidth_ > 0.0f && farmEditorWindowHeight_ > 0.0f;
	}
#endif
}

void FarmDebugEditorWindow::SaveSettings()
{
#ifdef USE_IMGUI
	std::error_code error;
	std::filesystem::create_directories(std::filesystem::path(kSettingsPath).parent_path(), error);
	if (error) {
		Logger::Log("FarmDebugEditorWindow::SaveSettings failed. Could not create settings directory: " + error.message());
		return;
	}

	nlohmann::json json;
	json["showFarmDebugWindow"] = showFarmDebugWindow_;
	json[kFarmEditorWindowKey] = {
		{ "x", farmEditorWindowX_ },
		{ "y", farmEditorWindowY_ },
		{ "width", farmEditorWindowWidth_ },
		{ "height", farmEditorWindowHeight_ },
	};

	JsonFile::Save(kSettingsPath, json);
#endif
}

void FarmDebugEditorWindow::Draw(
	FarmGrid& grid,
	FarmToolActionSystem& toolActionSystem)
{
#ifdef USE_IMGUI
	if (applyWindowPlacement_) {
		ImGui::SetNextWindowPos(ImVec2(farmEditorWindowX_, farmEditorWindowY_), ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(farmEditorWindowWidth_, farmEditorWindowHeight_), ImGuiCond_Always);
		applyWindowPlacement_ = false;
	}

	ImGui::Begin("Farm Editor");

	ImGui::BeginChild("FarmEditorToolbar", ImVec2(0.0f, 40.0f), true);
	ImGui::TextUnformatted("Farm Editor");
	ImGui::SameLine();
	ImGui::TextDisabled("Debug Mode");
	ImGui::SameLine();
	if (ImGui::Button("Save Editor Settings")) {
		SaveSettings();
	}
	ImGui::SameLine();
	if (ImGui::Button("Load Editor Settings")) {
		LoadSettings();
	}
	ImGui::EndChild();

	const float hierarchyWidth = 180.0f;
	ImGui::BeginChild("FarmEditorHierarchy", ImVec2(hierarchyWidth, 0.0f), true);
	ImGui::TextUnformatted("Hierarchy");
	ImGui::Separator();
	if (ImGui::TreeNodeEx("Farm", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::BulletText("Grid");
		ImGui::BulletText("Selected Tile");
		ImGui::BulletText("Tool Action");
		ImGui::TreePop();
	}
	if (ImGui::TreeNodeEx("Debug Windows", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Checkbox("Farm Debug Window", &showFarmDebugWindow_);
		ImGui::TreePop();
	}
	ImGui::EndChild();

	ImGui::SameLine();

	ImGui::BeginChild("FarmEditorInspector", ImVec2(0.0f, 0.0f), true);
	ImGui::TextUnformatted("Inspector / Details");
	ImGui::Separator();
	ImGui::TextUnformatted("Selected Tile Details");
	ImGui::Separator();

	ImGui::Columns(2, "FarmSelectedTileDetails", false);
	ImGui::TextUnformatted("Index");
	ImGui::NextColumn();
	ImGui::Text("%d", grid.GetSelectedIndex());
	ImGui::NextColumn();

	if (const FarmTile* selectedTile = grid.GetSelectedTile()) {
		ImGui::TextUnformatted("Height");
		ImGui::NextColumn();
		ImGui::Text("%d", selectedTile->heightLevel);
		ImGui::NextColumn();

		ImGui::TextUnformatted("State");
		ImGui::NextColumn();
		ImGui::TextUnformatted(ToString(selectedTile->state));
		ImGui::NextColumn();

		ImGui::TextUnformatted("Water");
		ImGui::NextColumn();
		ImGui::Text("%.0f%%", selectedTile->moisture * 100.0f);
		ImGui::NextColumn();

		ImGui::TextUnformatted("Crop");
		ImGui::NextColumn();
		ImGui::TextUnformatted(ToString(selectedTile->crop));
		ImGui::NextColumn();

		ImGui::TextUnformatted("Growth");
		ImGui::NextColumn();
		ImGui::Text("%.2f", selectedTile->growth);
		ImGui::NextColumn();
	} else {
		ImGui::TextUnformatted("Status");
		ImGui::NextColumn();
		ImGui::TextUnformatted("Tile Invalid");
		ImGui::NextColumn();
	}
	ImGui::Columns(1);
	ImGui::EndChild();

	const ImVec2 windowPos = ImGui::GetWindowPos();
	const ImVec2 windowSize = ImGui::GetWindowSize();
	farmEditorWindowX_ = windowPos.x;
	farmEditorWindowY_ = windowPos.y;
	farmEditorWindowWidth_ = windowSize.x;
	farmEditorWindowHeight_ = windowSize.y;

	ImGui::End();

	if (showFarmDebugWindow_) {
		farmDebugWindow_.Draw(grid, toolActionSystem);
	}
#else
	(void)grid;
	(void)toolActionSystem;
#endif
}

} // namespace farm
