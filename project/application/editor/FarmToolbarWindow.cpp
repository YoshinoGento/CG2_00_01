#include "editor/FarmToolbarWindow.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>
#include <string_view>
#include <utility>

#ifdef USE_IMGUI
#include "base/ImGuiManager.h"
#include "externals/imgui/imgui_internal.h"

namespace {
bool ContainsSearchText(std::string_view text, std::string_view search) {
	if (search.empty()) {
		return true;
	}
	const auto match = std::search(
		text.begin(), text.end(), search.begin(), search.end(),
		[](char left, char right) {
			const auto leftByte = static_cast<unsigned char>(left);
			const auto rightByte = static_cast<unsigned char>(right);
			if (leftByte < 0x80u && rightByte < 0x80u) {
				return std::tolower(leftByte) == std::tolower(rightByte);
			}
			return leftByte == rightByte;
		});
	return match != text.end();
}
} // namespace
#endif

void FarmToolbarWindow::PrepareSaveAsName(const std::string& suggestedName) {
	saveAsNameBuffer_.fill('\0');
	const std::string& name = suggestedName == "Untitled Farm" ? std::string{} : suggestedName;
	std::snprintf(saveAsNameBuffer_.data(), saveAsNameBuffer_.size(), "%s", name.c_str());
}

void FarmToolbarWindow::RequestSaveAsDialog(const std::string& suggestedName) {
	PrepareSaveAsName(suggestedName);
	openSaveAsNextFrame_ = true;
}

FarmToolbarActions FarmToolbarWindow::Draw(
	const editor::GamePlayEditorViewModel& viewModel,
	EditorLanguage language,
	bool gameMode) {
	FarmToolbarActions actions;
#ifdef USE_IMGUI
	const auto text = [language](std::string_view english) {
		return editor::Localize(language, english);
	};
	constexpr ImGuiWindowFlags kToolbarFlags =
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_HorizontalScrollbar |
		ImGuiWindowFlags_NoSavedSettings;
	const float toolbarHeight = ImGui::GetFrameHeightWithSpacing() * 2.0f +
		ImGui::GetStyle().WindowPadding.y * 2.0f + ImGui::GetStyle().ScrollbarSize;
	if (!ImGui::BeginViewportSideBar("FarmToolbar", ImGui::GetMainViewport(),
		ImGuiDir_Up, toolbarHeight, kToolbarFlags)) {
		ImGui::End();
		return actions;
	}

	if (openSaveAsNextFrame_) {
		ImGui::OpenPopup(text("Save Farm As###FarmSaveAs"));
		openSaveAsNextFrame_ = false;
	}
	if (openDiscardNextFrame_) {
		ImGui::OpenPopup(text("Discard unsaved Farm edits?###FarmDiscardChanges"));
		openDiscardNextFrame_ = false;
	}
	if (openLoadNextFrame_) {
		const bool selectionStillExists = std::any_of(
			viewModel.farmDocuments.begin(), viewModel.farmDocuments.end(),
			[&](const editor::FarmDocumentEditorViewData& document) {
				return document.id == selectedLoadDocumentId_;
			});
		if (!selectionStillExists) {
			selectedLoadDocumentId_ = !viewModel.farmDocumentActiveId.empty()
				? viewModel.farmDocumentActiveId
				: (viewModel.farmDocuments.empty() ? std::string{} : viewModel.farmDocuments.front().id);
		}
		loadSearchBuffer_.fill('\0');
		renameMode_ = false;
		ImGui::OpenPopup(text("Farm Saves###FarmSaves"));
		openLoadNextFrame_ = false;
	}

	ImGui::BeginDisabled(viewModel.irrigationPreviewActive);
	if (ImGui::RadioButton(text("Development Mode"), !gameMode)) { actions.gameMode = false; }
	ImGui::SameLine();
	if (ImGui::RadioButton(text("Game Mode"), gameMode)) { actions.gameMode = true; }
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
		ImGui::SetTooltip("%s", text(viewModel.irrigationPreviewActive
			? "Finish or cancel the preview before switching modes"
			: "Modes share the current Farm; switching does not reset or save it"));
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	ImGui::TextDisabled("|");
	ImGui::SameLine();
	if (ImGui::Button(viewModel.simulation.paused ? ">##Resume" : "||##Pause", {30.0f, 0.0f})) {
		actions.simulationAction = viewModel.simulation.paused
			? editor::SimulationEditorAction::Play : editor::SimulationEditorAction::Pause;
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("%s", text(viewModel.simulation.paused ? "Resume simulation" : "Pause simulation"));
	}
	ImGui::SameLine();
	ImGui::TextUnformatted(text(viewModel.simulation.paused ? "PAUSED" : "PLAYING"));
	ImGui::SameLine();
	if (ImGui::Button(text("New"))) {
		if (viewModel.farmDocumentDirty) {
			pendingDocumentAction_ = PendingDocumentAction::NewDocument;
			ImGui::OpenPopup(text("Discard unsaved Farm edits?###FarmDiscardChanges"));
		} else { actions.newDocument = true; }
	}
	ImGui::SameLine();
	if (ImGui::Button(text("Saves..."))) { openLoadNextFrame_ = true; }
	ImGui::SameLine();
	const bool canDirectSave = !viewModel.farmDocumentExists ||
		viewModel.farmDocumentDirty || viewModel.farmDocumentHasError;
	ImGui::BeginDisabled(!canDirectSave);
	if (ImGui::Button(text("Save"))) {
		if (viewModel.farmDocumentExists) { actions.saveDocument = true; }
		else { RequestSaveAsDialog(viewModel.farmDocumentName); }
	}
	ImGui::EndDisabled();
	ImGui::SameLine();
	if (ImGui::Button(text("Save As..."))) { RequestSaveAsDialog(viewModel.farmDocumentName); }

	const char* documentState = viewModel.farmDocumentHasError ? text("ERROR")
		: viewModel.farmDocumentDirty ? text("UNSAVED") : text("SAVED");
	// Clip long user names to a fixed slot so commands remain reachable.
	const ImVec2 nameStart = ImGui::GetCursorScreenPos();
	const ImVec2 nameSize{240.0f, ImGui::GetFrameHeight()};
	ImGui::GetWindowDrawList()->PushClipRect(nameStart, {nameStart.x + nameSize.x, nameStart.y + nameSize.y}, true);
	ImGui::GetWindowDrawList()->AddText(nameStart, ImGui::GetColorU32(ImGuiCol_Text),
		viewModel.farmDocumentName.c_str());
	ImGui::GetWindowDrawList()->PopClipRect();
	ImGui::Dummy(nameSize);
	if (ImGui::IsItemHovered()) { ImGui::SetTooltip("%s", viewModel.farmDocumentName.c_str()); }
	ImGui::SameLine();
	ImGui::TextColored(viewModel.farmDocumentHasError ? ImVec4(1,0.3f,0.2f,1)
		: viewModel.farmDocumentDirty ? ImVec4(1,0.75f,0.2f,1) : ImVec4(0.4f,0.9f,0.55f,1),
		"%s", documentState);
	if (!gameMode) {
		ImGui::SameLine();
		ImGui::BeginDisabled(!viewModel.canUndo);
		if (ImGui::ArrowButton("##UndoFarm", ImGuiDir_Left)) { actions.undo = true; }
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
			ImGui::SetTooltip("%s: %s", text("Undo"), text(viewModel.undoName));
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::BeginDisabled(!viewModel.canRedo);
		if (ImGui::ArrowButton("##RedoFarm", ImGuiDir_Right)) { actions.redo = true; }
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
			ImGui::SetTooltip("%s: %s", text("Redo"), text(viewModel.redoName));
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::BeginDisabled(!viewModel.simulation.paused);
		if (ImGui::Button(">|##SimulationStep", {30.0f, 0.0f})) {
			actions.simulationAction = editor::SimulationEditorAction::Step;
		}
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
			ImGui::SetTooltip("%s", text("Advance one simulation step"));
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		if (ImGui::Button(text("Reset Layout"))) { actions.resetLayout = true; }
	}

	if (ImGui::BeginPopupModal(
		text("Discard unsaved Farm edits?###FarmDiscardChanges"),
		nullptr,
		ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::TextUnformatted(text("The current Farm has unsaved changes."));
		ImGui::TextUnformatted(text("Continuing will discard those edits."));
		ImGui::Spacing();
		if (ImGui::Button(text("Cancel"), ImVec2(100.0f, 0.0f))) {
			pendingDocumentAction_ = PendingDocumentAction::None;
			pendingLoadDocumentId_.clear();
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.72f, 0.25f, 0.18f, 1.0f));
		if (ImGui::Button(text("Discard"), ImVec2(140.0f, 0.0f))) {
			if (pendingDocumentAction_ == PendingDocumentAction::NewDocument) {
				actions.newDocument = true;
			} else if (pendingDocumentAction_ == PendingDocumentAction::LoadDocument) {
				actions.loadDocumentId = pendingLoadDocumentId_;
			}
			pendingDocumentAction_ = PendingDocumentAction::None;
			pendingLoadDocumentId_.clear();
			ImGui::CloseCurrentPopup();
		}
		ImGui::PopStyleColor();
		ImGui::EndPopup();
	}

	ImGui::SetNextWindowSize(ImVec2(460.0f, 0.0f), ImGuiCond_Appearing);
	if (ImGui::BeginPopupModal(
		text("Save Farm As###FarmSaveAs"),
		nullptr,
		ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::TextUnformatted(text("Farm name"));
		ImGui::SetNextItemWidth(420.0f);
		const bool submitted = ImGui::InputText(
			"##FarmSaveAsName",
			saveAsNameBuffer_.data(),
			saveAsNameBuffer_.size(),
			ImGuiInputTextFlags_EnterReturnsTrue);
		ImGui::TextDisabled(
			"%s",
			text("Japanese names are stored as UTF-8. The filename remains safe and internal."));
		ImGui::Spacing();
		if (ImGui::Button(text("Cancel"), ImVec2(100.0f, 0.0f))) {
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		const bool hasName = saveAsNameBuffer_[0] != '\0';
		ImGui::BeginDisabled(!hasName);
		if (ImGui::Button(text("Save"), ImVec2(120.0f, 0.0f)) || (submitted && hasName)) {
			actions.saveAsName = std::string(saveAsNameBuffer_.data());
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndDisabled();
		ImGui::EndPopup();
	}

	ImGui::SetNextWindowSize(ImVec2(640.0f, 460.0f), ImGuiCond_Appearing);
	if (ImGui::BeginPopupModal(text("Farm Saves###FarmSaves"), nullptr)) {
		const bool selectionExists = std::any_of(
			viewModel.farmDocuments.begin(), viewModel.farmDocuments.end(),
			[&](const editor::FarmDocumentEditorViewData& document) {
				return document.id == selectedLoadDocumentId_;
			});
		if (!selectionExists) {
			selectedLoadDocumentId_ = !viewModel.farmDocumentActiveId.empty()
				? viewModel.farmDocumentActiveId
				: (viewModel.farmDocuments.empty()
					? std::string{}
					: viewModel.farmDocuments.front().id);
		}
		ImGui::Text(text("Saved Farms (%zu)"), viewModel.farmDocuments.size());
		ImGui::SameLine();
		ImGui::TextDisabled("%s", text("Select a save to load or rename it"));
		if (viewModel.farmDocumentHasError && !viewModel.farmDocumentMessage.empty()) {
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.34f, 0.26f, 1.0f));
			ImGui::TextWrapped("%s", text(viewModel.farmDocumentMessage));
			ImGui::PopStyleColor();
		}
		ImGui::SetNextItemWidth(-1.0f);
		ImGui::InputTextWithHint(
			"##FarmDocumentSearch",
			text("Search by name..."),
			loadSearchBuffer_.data(),
			loadSearchBuffer_.size());
		ImGui::Spacing();

		if (ImGui::BeginTable(
			"FarmDocumentTable",
			2,
			ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg |
				ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp,
			ImVec2(0.0f, renameMode_ ? 205.0f : 260.0f))) {
			ImGui::TableSetupColumn(text("Name"), ImGuiTableColumnFlags_WidthStretch, 0.65f);
			ImGui::TableSetupColumn(text("Saved"), ImGuiTableColumnFlags_WidthStretch, 0.35f);
			ImGui::TableHeadersRow();
			for (const editor::FarmDocumentEditorViewData& document : viewModel.farmDocuments) {
				if (!ContainsSearchText(document.displayName, loadSearchBuffer_.data())) {
					continue;
				}
				ImGui::PushID(document.id.c_str());
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				const bool selected = selectedLoadDocumentId_ == document.id;
				std::string label = document.active
					? std::string(text("[Open] ")) + document.displayName
					: document.displayName;
				if (ImGui::Selectable(
					label.c_str(), selected,
					ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick)) {
					selectedLoadDocumentId_ = document.id;
					renameMode_ = false;
					if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
						if (viewModel.farmDocumentDirty) {
							pendingDocumentAction_ = PendingDocumentAction::LoadDocument;
							pendingLoadDocumentId_ = document.id;
							openDiscardNextFrame_ = true;
						} else {
							actions.loadDocumentId = document.id;
						}
						ImGui::CloseCurrentPopup();
					}
				}
				ImGui::TableSetColumnIndex(1);
				ImGui::TextUnformatted(document.savedAt.c_str());
				ImGui::PopID();
			}
			ImGui::EndTable();
		}

		const auto selectedDocument = std::find_if(
			viewModel.farmDocuments.begin(), viewModel.farmDocuments.end(),
			[&](const editor::FarmDocumentEditorViewData& document) {
				return document.id == selectedLoadDocumentId_;
			});
		const bool hasSelection = selectedDocument != viewModel.farmDocuments.end();
		if (hasSelection) {
			ImGui::TextDisabled(
				text("Selected: %s%s"),
				selectedDocument->displayName.c_str(),
				selectedDocument->active ? text(" (currently open)") : "");
		} else if (viewModel.farmDocuments.empty()) {
			ImGui::TextDisabled("%s", text("No Farm saves yet. Use Save As to create one."));
		}

		if (ImGui::Button(text("Close"), ImVec2(100.0f, 0.0f))) {
			renameMode_ = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		ImGui::BeginDisabled(!hasSelection);
		if (ImGui::Button(text("Load Selected"), ImVec2(180.0f, 0.0f))) {
			if (viewModel.farmDocumentDirty) {
				pendingDocumentAction_ = PendingDocumentAction::LoadDocument;
				pendingLoadDocumentId_ = selectedLoadDocumentId_;
				openDiscardNextFrame_ = true;
			} else {
				actions.loadDocumentId = selectedLoadDocumentId_;
			}
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		bool renameStarted = false;
		if (ImGui::Button(text("Rename..."), ImVec2(120.0f, 0.0f))) {
			renameNameBuffer_.fill('\0');
			std::snprintf(
				renameNameBuffer_.data(),
				renameNameBuffer_.size(),
				"%s",
				selectedDocument->displayName.c_str());
			renameMode_ = true;
			renameStarted = true;
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::BeginDisabled(!hasSelection || renameMode_);
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.66f, 0.18f, 0.16f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.82f, 0.24f, 0.20f, 1.0f));
		if (ImGui::Button(text("Delete..."), ImVec2(100.0f, 0.0f))) {
			pendingDeleteDocumentId_ = selectedDocument->id;
			pendingDeleteDocumentName_ = selectedDocument->displayName;
			ImGui::OpenPopup(text("Delete Farm Save###FarmDeleteSave"));
		}
		ImGui::PopStyleColor(2);
		ImGui::EndDisabled();
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
			ImGui::SetTooltip("%s", text("Delete selected save"));
		}

		if (renameMode_ && hasSelection) {
			ImGui::SeparatorText(text("Rename selected save"));
			if (renameStarted) {
				ImGui::SetKeyboardFocusHere();
			}
			ImGui::SetNextItemWidth(-1.0f);
			const bool submitted = ImGui::InputText(
				"##FarmRenameName",
				renameNameBuffer_.data(),
				renameNameBuffer_.size(),
				ImGuiInputTextFlags_EnterReturnsTrue);
			ImGui::TextDisabled("%s", text("The internal save ID and file path will not change."));
			if (ImGui::Button(text("Cancel Rename"), ImVec2(150.0f, 0.0f))) {
				renameMode_ = false;
			}
			ImGui::SameLine();
			const bool hasRename = renameNameBuffer_[0] != '\0';
			ImGui::BeginDisabled(!hasRename);
			if (ImGui::Button(text("Apply Rename"), ImVec2(140.0f, 0.0f)) ||
				(submitted && hasRename)) {
				FarmDocumentRenameAction renameAction;
				renameAction.documentId = selectedLoadDocumentId_;
				renameAction.displayName = std::string(renameNameBuffer_.data());
				actions.renameDocument = std::move(renameAction);
				renameMode_ = false;
			}
			ImGui::EndDisabled();
		}

		if (ImGui::BeginPopupModal(
			text("Delete Farm Save###FarmDeleteSave"),
			nullptr,
			ImGuiWindowFlags_AlwaysAutoResize)) {
			ImGui::Text(text("Delete \"%s\"?"), pendingDeleteDocumentName_.c_str());
			ImGui::TextUnformatted(text("This operation cannot be undone."));
			if (pendingDeleteDocumentId_ == viewModel.farmDocumentActiveId) {
				ImGui::Spacing();
				ImGui::TextWrapped(
					"%s",
					text("The current Farm will remain open as an unsaved document."));
			}
			ImGui::Spacing();
			if (ImGui::Button(text("Cancel"), ImVec2(110.0f, 0.0f))) {
				pendingDeleteDocumentId_.clear();
				pendingDeleteDocumentName_.clear();
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.72f, 0.18f, 0.16f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.88f, 0.24f, 0.20f, 1.0f));
			if (ImGui::Button(text("Delete Save"), ImVec2(150.0f, 0.0f))) {
				actions.deleteDocumentId = pendingDeleteDocumentId_;
				selectedLoadDocumentId_.clear();
				renameMode_ = false;
				pendingDeleteDocumentId_.clear();
				pendingDeleteDocumentName_.clear();
				ImGui::CloseCurrentPopup();
			}
			ImGui::PopStyleColor(2);
			ImGui::EndPopup();
		}
		ImGui::EndPopup();
	}

	ImGui::End();
#else
	(void)viewModel;
	(void)language;
	(void)gameMode;
#endif
	return actions;
}
