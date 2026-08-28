#include "editor/FarmMapWindow.h"

#ifdef USE_IMGUI
#include "base/ImGuiManager.h"

#include <algorithm>
#include <cstdio>
#endif

namespace {
#ifdef USE_IMGUI
enum class FarmMapVisualState {
	Empty,
	CanalDry,
	CanalSupplied,
	WaterSource,
	Tilled,
	Watered,
	Growing,
	Ready,
};

FarmMapVisualState GetVisualState(const editor::FarmTileEditorViewData& tile) noexcept {
	if (tile.feature == farm::FarmTileFeature::Canal) {
		return tile.irrigationSupplied
			? FarmMapVisualState::CanalSupplied
			: FarmMapVisualState::CanalDry;
	}
	if (tile.feature == farm::FarmTileFeature::WaterSource) {
		return FarmMapVisualState::WaterSource;
	}
	if (tile.canHarvest) {
		return FarmMapVisualState::Ready;
	}
	if (tile.state == farm::FarmTileState::Planted) {
		return FarmMapVisualState::Growing;
	}
	if (tile.state == farm::FarmTileState::Tilled) {
		return tile.moisture > 0.5f
			? FarmMapVisualState::Watered
			: FarmMapVisualState::Tilled;
	}
	return FarmMapVisualState::Empty;
}

ImVec4 GetStateColor(FarmMapVisualState state) noexcept {
	switch (state) {
	case FarmMapVisualState::CanalDry: return { 0.10f, 0.30f, 0.48f, 1.0f };
	case FarmMapVisualState::CanalSupplied: return { 0.08f, 0.72f, 0.98f, 1.0f };
	case FarmMapVisualState::WaterSource: return { 0.14f, 0.90f, 1.0f, 1.0f };
	case FarmMapVisualState::Tilled: return { 0.68f, 0.43f, 0.22f, 1.0f };
	case FarmMapVisualState::Watered: return { 0.30f, 0.52f, 0.72f, 1.0f };
	case FarmMapVisualState::Growing: return { 0.28f, 0.68f, 0.30f, 1.0f };
	case FarmMapVisualState::Ready: return { 0.95f, 0.72f, 0.16f, 1.0f };
	case FarmMapVisualState::Empty:
	default: return { 0.34f, 0.58f, 0.28f, 1.0f };
	}
}

const char* GetStateName(FarmMapVisualState state, EditorLanguage language) noexcept {
	const char* stateName = "Empty";
	switch (state) {
	case FarmMapVisualState::CanalDry: stateName = "Dry Canal"; break;
	case FarmMapVisualState::CanalSupplied: stateName = "Supplied"; break;
	case FarmMapVisualState::WaterSource: stateName = "Water Source"; break;
	case FarmMapVisualState::Tilled: stateName = "Tilled"; break;
	case FarmMapVisualState::Watered: stateName = "Watered"; break;
	case FarmMapVisualState::Growing: stateName = "Growing"; break;
	case FarmMapVisualState::Ready: stateName = "Ready"; break;
	case FarmMapVisualState::Empty:
	default: break;
	}
	return editor::Localize(language, stateName);
}

ImVec4 Brighten(const ImVec4& color, float amount) noexcept {
	return {
		(std::min)(color.x + amount, 1.0f),
		(std::min)(color.y + amount, 1.0f),
		(std::min)(color.z + amount, 1.0f),
		color.w,
	};
}

void DrawLegendItem(const char* label, const ImVec4& color) {
	ImGui::ColorButton(label, color, ImGuiColorEditFlags_NoTooltip, { 10.0f, 10.0f });
	ImGui::SameLine(0.0f, 4.0f);
	ImGui::TextDisabled("%s", label);
}
#endif
} // namespace

FarmMapActions FarmMapWindow::Draw(
	const editor::GamePlayEditorViewModel& viewModel,
	const editor::EditorSelection& selection,
	EditorLanguage language)
{
	FarmMapActions actions;
#ifdef USE_IMGUI
	if (!open_) {
		return actions;
	}
	const auto text = [language](std::string_view english) {
		return editor::Localize(language, english);
	};
	if (!ImGui::Begin(text("Farm Map###FarmMap"), &open_, ImGuiWindowFlags_NoCollapse)) {
		ImGui::End();
		return actions;
	}

	if (viewModel.farmWidth <= 0 || viewModel.farmHeight <= 0 || viewModel.farmTiles.empty()) {
		ImGui::TextDisabled("%s", text("Farm data is unavailable."));
		ImGui::End();
		return actions;
	}

	ImGui::Text(text("Farm Grid  %d x %d"), viewModel.farmWidth, viewModel.farmHeight);
	ImGui::TextDisabled("%s", text("Click a tile to select it"));
	ImGui::Spacing();

	if (ImGui::BeginTable(
		"FarmMapGrid",
		viewModel.farmWidth,
		ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoPadOuterX)) {
		for (const editor::FarmTileEditorViewData& tile : viewModel.farmTiles) {
			ImGui::TableNextColumn();
			const bool selected =
				tile.index == viewModel.selectedFarmTileIndex ||
				(selection.type == editor::EditorSelectionType::FarmTile &&
				 selection.generation == viewModel.farmGeneration &&
				 selection.index == tile.index);
			const FarmMapVisualState visualState = GetVisualState(tile);
			const ImVec4 tileColor = GetStateColor(visualState);

			ImGui::PushID(tile.index);
			ImGui::PushStyleColor(ImGuiCol_Button, tileColor);
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Brighten(tileColor, 0.12f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, Brighten(tileColor, 0.05f));
			ImGui::PushStyleColor(
				ImGuiCol_Border,
				selected ? ImVec4(1.0f, 0.90f, 0.10f, 1.0f) : ImVec4(0.0f, 0.0f, 0.0f, 0.40f));
			ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, selected ? 3.0f : 1.0f);

			char label[48]{};
			std::snprintf(
				label,
				sizeof(label),
				"%02d\n%s\nH%d",
				tile.index,
				GetStateName(visualState, language),
				tile.heightLevel);
			if (ImGui::Button(label, { -1.0f, 58.0f })) {
				actions.selectedTileIndex = tile.index;
			}

			const ImVec2 rectMin = ImGui::GetItemRectMin();
			const ImVec2 rectMax = ImGui::GetItemRectMax();
			const float barLeft = rectMin.x + 3.0f;
			const float barWidth = (std::max)(rectMax.x - rectMin.x - 6.0f, 0.0f);
			ImDrawList* drawList = ImGui::GetWindowDrawList();
			const float moisture = std::clamp(tile.moisture, 0.0f, 1.0f);
			const float growth = std::clamp(tile.growth, 0.0f, 1.0f);
			drawList->AddRectFilled(
				{ barLeft, rectMax.y - 4.0f },
				{ barLeft + barWidth * moisture, rectMax.y - 2.0f },
				IM_COL32(70, 170, 255, 255));
			drawList->AddRectFilled(
				{ barLeft, rectMax.y - 7.0f },
				{ barLeft + barWidth * growth, rectMax.y - 5.0f },
				visualState == FarmMapVisualState::Ready
					? IM_COL32(255, 205, 45, 255)
					: IM_COL32(75, 220, 105, 255));

			if (ImGui::IsItemHovered()) {
				ImGui::BeginTooltip();
				ImGui::Text(text("Tile %d  (%d, %d)"), tile.index, tile.column, tile.row);
				ImGui::Text(text("State: %s"), GetStateName(visualState, language));
				ImGui::Text(text("Height: H%d"), tile.heightLevel);
				ImGui::Text(text("Moisture: %.0f%%"), moisture * 100.0f);
				ImGui::Text(text("Growth: %.0f%%"), growth * 100.0f);
				ImGui::EndTooltip();
			}

			ImGui::PopStyleVar();
			ImGui::PopStyleColor(4);
			ImGui::PopID();
		}
		ImGui::EndTable();
	}

	ImGui::Spacing();
	ImGui::SeparatorText(text("Legend"));
	DrawLegendItem(text("Empty"), GetStateColor(FarmMapVisualState::Empty));
	DrawLegendItem(text("Dry Canal"), GetStateColor(FarmMapVisualState::CanalDry));
	DrawLegendItem(text("Supplied"), GetStateColor(FarmMapVisualState::CanalSupplied));
	DrawLegendItem(text("Water Source"), GetStateColor(FarmMapVisualState::WaterSource));
	DrawLegendItem(text("Tilled"), GetStateColor(FarmMapVisualState::Tilled));
	DrawLegendItem(text("Watered"), GetStateColor(FarmMapVisualState::Watered));
	DrawLegendItem(text("Growing"), GetStateColor(FarmMapVisualState::Growing));
	DrawLegendItem(text("Ready"), GetStateColor(FarmMapVisualState::Ready));
	ImGui::TextDisabled("%s", text("Blue bar: moisture"));
	ImGui::TextDisabled("%s", text("Green/gold bar: growth"));
	ImGui::End();
#else
	(void)viewModel;
	(void)selection;
	(void)language;
#endif
	return actions;
}
