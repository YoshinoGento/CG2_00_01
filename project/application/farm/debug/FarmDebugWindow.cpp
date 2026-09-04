#include "farm/debug/FarmDebugWindow.h"

#include "farm/core/FarmGrid.h"
#include "farm/system/FarmToolActionSystem.h"

#include <string_view>

#ifdef USE_IMGUI
#include "base/ImGuiManager.h"
#endif

namespace farm {

void FarmDebugWindow::Draw(
	FarmGrid& grid,
	FarmToolActionSystem& toolActionSystem)
{
#ifdef USE_IMGUI
	ImGui::Begin("Farm Debug");

	ImGui::Text("Grid: %d x %d", grid.GetWidth(), grid.GetHeight());
	ImGui::Text("Selected Index: %d", grid.GetSelectedIndex());

	if (const FarmTile* selectedTile = grid.GetSelectedTile()) {
		ImGui::Separator();
		ImGui::TextUnformatted("Selected Tile");
		ImGui::Text("Height Level: %d", selectedTile->heightLevel);
		ImGui::Text("State: %s", ToString(selectedTile->state));
		ImGui::Text("Crop: %s", ToString(selectedTile->crop));
		ImGui::Text("Moisture: %.2f", selectedTile->moisture);
		ImGui::Text("Growth: %.2f", selectedTile->growth);
	} else {
		ImGui::TextUnformatted("Selected Tile: Invalid");
	}

	ImGui::Separator();
	ImGui::TextUnformatted("Grid Controls");
	if (ImGui::Button("Move Left")) {
		grid.MoveSelection(-1, 0);
	}
	ImGui::SameLine();
	if (ImGui::Button("Move Right")) {
		grid.MoveSelection(1, 0);
	}
	if (ImGui::Button("Move Up")) {
		grid.MoveSelection(0, -1);
	}
	ImGui::SameLine();
	if (ImGui::Button("Move Down")) {
		grid.MoveSelection(0, 1);
	}

	ImGui::Separator();
	ImGui::TextUnformatted("Tile Controls");
	if (ImGui::Button("Raise Height")) {
		toolActionSystem.RaiseSelectedTile(grid);
	}
	ImGui::SameLine();
	if (ImGui::Button("Lower Height")) {
		toolActionSystem.LowerSelectedTile(grid);
	}
	if (ImGui::Button("Apply Hoe")) {
		static_cast<void>(toolActionSystem.ApplyTool(grid, FarmTool::Hoe));
	}
	ImGui::SameLine();
	if (ImGui::Button("Apply Water")) {
		static_cast<void>(toolActionSystem.ApplyTool(grid, FarmTool::Water));
	}
	ImGui::SameLine();
	if (ImGui::Button("Apply Seed")) {
		static_cast<void>(toolActionSystem.ApplyTool(grid, FarmTool::Seed));
	}

	ImGui::Separator();
	const CommandHistory& history = toolActionSystem.GetHistory();
	ImGui::BeginDisabled(!history.CanUndo());
	if (ImGui::Button("Undo (Ctrl+Z)")) {
		toolActionSystem.Undo();
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::BeginDisabled(!history.CanRedo());
	if (ImGui::Button("Redo (Ctrl+Y)")) {
		toolActionSystem.Redo();
	}
	ImGui::EndDisabled();
	ImGui::Text("History: %zu undo / %zu redo", history.GetUndoCount(), history.GetRedoCount());
	if (history.CanUndo()) {
		const std::string_view name = history.GetUndoName();
		ImGui::Text("Next Undo: %.*s", static_cast<int>(name.size()), name.data());
	}
	if (history.CanRedo()) {
		const std::string_view name = history.GetRedoName();
		ImGui::Text("Next Redo: %.*s", static_cast<int>(name.size()), name.data());
	}

	ImGui::End();
#else
	(void)grid;
	(void)toolActionSystem;
#endif
}

} // namespace farm
