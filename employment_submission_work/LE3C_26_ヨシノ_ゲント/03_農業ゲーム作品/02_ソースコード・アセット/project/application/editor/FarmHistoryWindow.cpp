#include "editor/FarmHistoryWindow.h"

#ifdef USE_IMGUI
#include "base/ImGuiManager.h"
#endif

FarmHistoryActions FarmHistoryWindow::Draw(
	const editor::GamePlayEditorViewModel& viewModel,
	EditorLanguage language) {
	FarmHistoryActions actions;
#ifdef USE_IMGUI
	if (!open_) {
		return actions;
	}
	const auto text = [language](std::string_view english) {
		return editor::Localize(language, english);
	};
	if (!ImGui::Begin(
		text("Farm History###FarmHistory"),
		&open_,
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoScrollWithMouse)) {
		ImGui::End();
		return actions;
	}

	ImGui::BeginDisabled(!viewModel.canUndo);
	if (ImGui::Button(text("Undo"))) {
		actions.undo = true;
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::BeginDisabled(!viewModel.canRedo);
	if (ImGui::Button(text("Redo"))) {
		actions.redo = true;
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::TextDisabled(
		text("Undo %zu   Redo %zu   |   Ctrl+Z / Ctrl+Y"),
		viewModel.undoCount,
		viewModel.redoCount);

	ImGui::TextDisabled(
		text("Next Undo: %s    |    Next Redo: %s"),
		viewModel.canUndo ? text(viewModel.undoName) : text("None"),
		viewModel.canRedo ? text(viewModel.redoName) : text("None"));
	ImGui::End();
#else
	(void)viewModel;
	(void)language;
#endif
	return actions;
}
