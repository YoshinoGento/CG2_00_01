#include "editor/FarmControllerWindow.h"

#ifdef USE_IMGUI
#include "base/ImGuiManager.h"

#include <algorithm>
#include <array>
#endif

namespace {
#ifdef USE_IMGUI
struct ToolButtonData {
	FarmTool tool;
	const char* label;
};

constexpr std::array<ToolButtonData, 4> kToolButtons = {{
	{ FarmTool::Hoe, "1  Hoe" },
	{ FarmTool::Water, "2  Water" },
	{ FarmTool::Seed, "3  Seed" },
	{ FarmTool::Harvest, "4  Harvest" },
}};

const char* GetActionStatusLabel(FarmToolActionStatus status, EditorLanguage language)
{
	const char* label = "Tool does not match tile state";
	switch (status) {
	case FarmToolActionStatus::Applied:
	case FarmToolActionStatus::Harvested:
		label = "Action available";
		break;
	case FarmToolActionStatus::AlreadyWatered:
		label = "Already fully watered";
		break;
	case FarmToolActionStatus::NotReady:
		label = "Crop is not ready";
		break;
	case FarmToolActionStatus::InvalidTarget:
		label = "No valid tile";
		break;
	case FarmToolActionStatus::UnsupportedTool:
		label = "Tool is unavailable";
		break;
	case FarmToolActionStatus::InvalidState:
	default:
		break;
	}
	return editor::Localize(language, label);
}

const char* GetStateLabel(
	const editor::FarmTileEditorViewData& tile,
	EditorLanguage language) noexcept {
	const char* label = farm::ToString(tile.state);
	if (tile.canHarvest) {
		label = "Ready";
	} else if (tile.state == farm::FarmTileState::Planted) {
		label = "Growing";
	} else if (tile.state == farm::FarmTileState::Tilled && tile.moisture > 0.5f) {
		label = "Watered";
	}
	return editor::Localize(language, label);
}

ImVec4 GetStateColor(const editor::FarmTileEditorViewData& tile) noexcept {
	if (tile.canHarvest) {
		return { 0.95f, 0.72f, 0.16f, 1.0f };
	}
	if (tile.state == farm::FarmTileState::Planted) {
		return { 0.28f, 0.78f, 0.34f, 1.0f };
	}
	if (tile.state == farm::FarmTileState::Tilled) {
		return tile.moisture > 0.5f
			? ImVec4(0.30f, 0.58f, 0.88f, 1.0f)
			: ImVec4(0.72f, 0.45f, 0.24f, 1.0f);
	}
	return { 0.40f, 0.72f, 0.32f, 1.0f };
}

const char* GetNextActionLabel(
	const editor::FarmTileEditorViewData& tile,
	EditorLanguage language) noexcept {
	const char* label = "Growing: wait until the crop is ready.";
	if (tile.canHarvest) {
		label = "Next: select Harvest and apply.";
	} else if (tile.state == farm::FarmTileState::Empty) {
		label = "Next: select Hoe to prepare this tile.";
	} else if (tile.state == farm::FarmTileState::Tilled) {
		label = tile.moisture > 0.5f
			? "Next: select Seed to plant."
			: "Next: Water this tile or plant a Seed.";
	} else if (tile.moisture < 0.25f) {
		label = "Growing slowly: add Water.";
	}
	return editor::Localize(language, label);
}
#endif
} // namespace

FarmControllerActions FarmControllerWindow::Draw(
	const editor::GamePlayEditorViewModel& viewModel,
	EditorLanguage language)
{
	FarmControllerActions actions;
#ifdef USE_IMGUI
	if (!open_) {
		return actions;
	}
	const auto text = [language](std::string_view english) {
		return editor::Localize(language, english);
	};
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(
		{ viewport->WorkPos.x + (std::max)(viewport->WorkSize.x - 292.0f, 0.0f),
			viewport->WorkPos.y },
		ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize({ 292.0f, 370.0f }, ImGuiCond_FirstUseEver);
	if (!ImGui::Begin(text("Farm Inspector###FarmInspector"), &open_, ImGuiWindowFlags_NoCollapse)) {
		ImGui::End();
		return actions;
	}

	const int selectedIndex = viewModel.selectedFarmTileIndex;
	const editor::FarmTileEditorViewData* tile =
		selectedIndex >= 0 && selectedIndex < static_cast<int>(viewModel.farmTiles.size())
		? &viewModel.farmTiles[static_cast<std::size_t>(selectedIndex)]
		: nullptr;

	ImGui::SeparatorText(text("Selection"));
	if (tile != nullptr) {
		ImGui::Text(text("Tile #%d"), tile->index);
		ImGui::SameLine();
		ImGui::TextDisabled(text("Coordinate (%d, %d)"), tile->column, tile->row);
		const ImVec4 stateColor = GetStateColor(*tile);
		ImGui::ColorButton(
			"##SelectedTileState",
			stateColor,
			ImGuiColorEditFlags_NoTooltip,
			{ 12.0f, 12.0f });
		ImGui::SameLine();
		ImGui::Text(text("State: %s"), GetStateLabel(*tile, language));
		ImGui::Text(text("Crop: %s"), text(farm::ToString(tile->crop)));

		ImGui::SeparatorText(text("Terrain"));
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(text("Height"));
		ImGui::SameLine();
		ImGui::BeginDisabled(tile->heightLevel <= FarmToolActionSystem::kMinimumHeightLevel);
		if (ImGui::Button("-##Height", { 34.0f, 0.0f })) {
			actions.lowerTile = true;
		}
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
			ImGui::SetTooltip("%s", text("Lower selected tile (Page Down)"));
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::Text("H%d", tile->heightLevel);
		ImGui::SameLine();
		ImGui::BeginDisabled(tile->heightLevel >= FarmToolActionSystem::kMaximumHeightLevel);
		if (ImGui::Button("+##Height", { 34.0f, 0.0f })) {
			actions.raiseTile = true;
		}
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
			ImGui::SetTooltip("%s", text("Raise selected tile (Page Up)"));
		}
		ImGui::EndDisabled();

		ImGui::SeparatorText(text("Crop Status"));
		const float moisture = std::clamp(tile->moisture, 0.0f, 1.0f);
		const float growth = std::clamp(tile->growth, 0.0f, 1.0f);
		ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.25f, 0.60f, 0.95f, 1.0f));
		ImGui::ProgressBar(moisture, ImVec2(-1.0f, 0.0f), text("Water"));
		ImGui::PopStyleColor();
		ImGui::PushStyleColor(
			ImGuiCol_PlotHistogram,
			tile->canHarvest
				? ImVec4(1.0f, 0.76f, 0.12f, 1.0f)
				: ImVec4(0.25f, 0.82f, 0.34f, 1.0f));
		ImGui::ProgressBar(growth, ImVec2(-1.0f, 0.0f), text("Growth"));
		ImGui::PopStyleColor();
		ImGui::TextDisabled("%s", GetNextActionLabel(*tile, language));
	} else {
		ImGui::TextDisabled("%s", text("None"));
	}

	ImGui::SeparatorText(text("Farm Tool"));
	for (std::size_t index = 0; index < kToolButtons.size(); ++index) {
		const ToolButtonData& button = kToolButtons[index];
		const bool selected = button.tool == viewModel.currentFarmTool;
		if (selected) {
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.62f, 0.30f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.24f, 0.72f, 0.38f, 1.0f));
		}
		if (ImGui::Button(text(button.label), ImVec2(92.0f, 0.0f))) {
			actions.selectedTool = button.tool;
		}
		if (selected) {
			ImGui::PopStyleColor(2);
		}
		if (index + 1 < kToolButtons.size() && index % 2 == 0) {
			ImGui::SameLine();
		}
	}

	const bool canApply = tile != nullptr && viewModel.selectedFarmAction.Succeeded();
	const ImVec4 statusColor = canApply
		? ImVec4(0.25f, 0.95f, 0.38f, 1.0f)
		: ImVec4(1.0f, 0.32f, 0.20f, 1.0f);
	ImGui::TextColored(
		statusColor,
		"%s",
		GetActionStatusLabel(viewModel.selectedFarmAction.status, language));
	ImGui::BeginDisabled(!canApply);
	if (ImGui::Button(text("Apply Selected Tool"), ImVec2(-1.0f, 34.0f))) {
		actions.applyCurrentTool = true;
	}
	ImGui::EndDisabled();

	ImGui::SeparatorText(text("Viewport Shortcuts"));
	ImGui::TextDisabled("%s", text("Click: select   Shift+Click: select and apply"));
	ImGui::TextDisabled("%s", text("Arrows: move   Q/E: tool   Enter: apply"));
	ImGui::End();
#else
	(void)viewModel;
	(void)language;
#endif
	return actions;
}
