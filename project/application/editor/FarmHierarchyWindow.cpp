#include "editor/FarmHierarchyWindow.h"

#ifdef USE_IMGUI
#include "base/ImGuiManager.h"

#include <algorithm>
#include <cstdio>
#endif

FarmHierarchyActions FarmHierarchyWindow::Draw(
	const editor::GamePlayEditorViewModel& viewModel,
	const editor::EditorSelection& selection,
	EditorLanguage language) {
	FarmHierarchyActions actions;
#ifdef USE_IMGUI
	if (!open_) {
		return actions;
	}
	const auto text = [language](std::string_view english) {
		return editor::Localize(language, english);
	};
	if (!ImGui::Begin(text("Farm Hierarchy###FarmHierarchy"), &open_, ImGuiWindowFlags_NoCollapse)) {
		ImGui::End();
		return actions;
	}

	ImGui::TextDisabled("%s", text("Scene / Farm"));
	if (viewModel.farmTiles.empty() || viewModel.farmWidth <= 0 || viewModel.farmHeight <= 0) {
		ImGui::Separator();
		ImGui::TextDisabled("%s", text("Farm data is unavailable."));
		ImGui::End();
		return actions;
	}

	const bool farmOpen = ImGui::TreeNodeEx(
		"FarmGridNode",
		ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth,
		text("Farm Grid  (%d x %d)"),
		viewModel.farmWidth,
		viewModel.farmHeight);
	if (farmOpen) {
		for (int row = 0; row < viewModel.farmHeight; ++row) {
			ImGui::PushID(row);
			const bool rowOpen = ImGui::TreeNodeEx(
				"FarmRowNode",
				ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth,
				text("Row %d"),
				row);
			if (rowOpen) {
				for (int column = 0; column < viewModel.farmWidth; ++column) {
					const int tileIndex = row * viewModel.farmWidth + column;
					if (tileIndex < 0 || tileIndex >= static_cast<int>(viewModel.farmTiles.size())) {
						continue;
					}
					const editor::FarmTileEditorViewData& tile =
						viewModel.farmTiles[static_cast<std::size_t>(tileIndex)];
					const bool selected = selection.type == editor::EditorSelectionType::FarmTile &&
						selection.generation == viewModel.farmGeneration &&
						selection.index == tile.index;
					ImGui::PushID(tile.index);
					char label[64]{};
					std::snprintf(
						label,
						sizeof(label),
						text("Tile %02d  [%s]"),
						tile.index,
						text(farm::ToString(tile.state)));
					if (ImGui::Selectable(label, selected)) {
						actions.selectedTileIndex = tile.index;
					}
					if (ImGui::IsItemHovered()) {
						ImGui::SetTooltip(
							text("Coordinate (%d, %d)\nMoisture %.0f%%\nGrowth %.0f%%"),
							tile.column,
							tile.row,
							std::clamp(tile.moisture, 0.0f, 1.0f) * 100.0f,
							std::clamp(tile.growth, 0.0f, 1.0f) * 100.0f);
					}
					ImGui::PopID();
				}
				ImGui::TreePop();
			}
			ImGui::PopID();
		}
		ImGui::TreePop();
	}

	ImGui::End();
#else
	(void)viewModel;
	(void)selection;
	(void)language;
#endif
	return actions;
}
