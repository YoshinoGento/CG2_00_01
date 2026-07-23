#pragma once

#include "editor/EditorLocalization.h"
#include "editor/GamePlayEditorBridge.h"

#include <array>
#include <optional>
#include <string>

struct FarmDocumentRenameAction {
	std::string documentId;
	std::string displayName;
};

struct FarmToolbarActions {
	bool newDocument = false;
	bool saveDocument = false;
	std::optional<std::string> loadDocumentId;
	std::optional<std::string> saveAsName;
	std::optional<FarmDocumentRenameAction> renameDocument;
	std::optional<std::string> deleteDocumentId;
	std::optional<FarmTool> selectedTool;
	std::optional<editor::SimulationEditorAction> simulationAction;
	bool undo = false;
	bool redo = false;
	bool resetLayout = false;
};

// Fixed Farm workspace toolbar. It emits actions through EditorShell only.
class FarmToolbarWindow final {
public:
	[[nodiscard]] FarmToolbarActions Draw(
		const editor::GamePlayEditorViewModel& viewModel,
		EditorLanguage language);
	void RequestSaveAsDialog(const std::string& suggestedName);

private:
	enum class PendingDocumentAction {
		None,
		NewDocument,
		LoadDocument,
	};
	void PrepareSaveAsName(const std::string& suggestedName);

	PendingDocumentAction pendingDocumentAction_ = PendingDocumentAction::None;
	std::array<char, 256> saveAsNameBuffer_{};
	std::array<char, 256> renameNameBuffer_{};
	std::array<char, 128> loadSearchBuffer_{};
	std::string selectedLoadDocumentId_;
	std::string pendingLoadDocumentId_;
	std::string pendingDeleteDocumentId_;
	std::string pendingDeleteDocumentName_;
	bool renameMode_ = false;
	bool openDiscardNextFrame_ = false;
	bool openSaveAsNextFrame_ = false;
	bool openLoadNextFrame_ = false;
};
