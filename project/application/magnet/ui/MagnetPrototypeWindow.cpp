#include "application/magnet/ui/MagnetPrototypeWindow.h"

#include "base/SrvManager.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui_internal.h"
#endif

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace magnet {
namespace {

constexpr char kDockspaceName[] = "MagnetPrototypeDockSpace";
constexpr char kHierarchyWindowName[] = "Stage Hierarchy###MagnetHierarchy";
constexpr char kViewportWindowName[] = "Game View###MagnetViewport";
constexpr char kInspectorWindowName[] = "Stage Inspector###MagnetInspector";
constexpr char kMonitorWindowName[] = "Simulation Monitor###MagnetMonitor";
constexpr float kLeftPanelRatio = 0.20f;
constexpr float kRightPanelRatio = 0.28f;
constexpr float kBottomPanelRatio = 0.22f;
constexpr float kMinimumButtonWidth = 80.0f;
constexpr float kConstraintWarningError = 0.08f;

} // namespace

MagnetPrototypeWindow::MagnetPrototypeWindow()
{
	std::snprintf(
		saveName_.data(),
		saveName_.size(),
		"stage_new");
}

MagnetPrototypeUiRequest MagnetPrototypeWindow::Draw(
	const MagnetPrototypeViewData& viewData,
	SrvManager* srvManager,
	uint32_t finalDisplaySrvIndex,
	float virtualWidth,
	float virtualHeight)
{
	MagnetPrototypeUiRequest request{};
#ifdef USE_IMGUI
	DrawMainMenuBar(viewData, request);

	const ImGuiID dockspaceId = ImGui::GetID(kDockspaceName);
	if (rebuildLayoutRequested_ || ImGui::DockBuilderGetNode(dockspaceId) == nullptr) {
		BuildDefaultLayout(dockspaceId);
	}
	ImGui::DockSpaceOverViewport(
		dockspaceId,
		ImGui::GetMainViewport(),
		ImGuiDockNodeFlags_None);

	DrawHierarchy(viewData);
	DrawViewport(
		srvManager,
		finalDisplaySrvIndex,
		virtualWidth,
		virtualHeight);
	DrawInspector(viewData, request);
	DrawMonitor(viewData);
#else
	(void)viewData;
	(void)srvManager;
	(void)finalDisplaySrvIndex;
	(void)virtualWidth;
	(void)virtualHeight;
#endif

	request.showGrid = showGrid_;
	request.showVelocity = showVelocity_;
	request.cameraFollow = cameraFollow_;
	return request;
}

void MagnetPrototypeWindow::DrawMainMenuBar(
	const MagnetPrototypeViewData& viewData,
	MagnetPrototypeUiRequest& request)
{
#ifdef USE_IMGUI
	if (!ImGui::BeginMainMenuBar()) {
		return;
	}
	ImGui::TextUnformatted("MAGNET STAGE EDITOR");
	ImGui::Separator();
	if (ImGui::BeginMenu("Stage")) {
		if (ImGui::MenuItem("Refresh Save List")) {
			request.stageAction = MagnetStageEditorAction::RefreshSaves;
		}
		ImGui::Separator();
		if (ImGui::MenuItem("Generate Balanced Random")) {
			request.stageAction = MagnetStageEditorAction::GenerateBalanced;
			request.generationSettings = generationSettings_;
		}
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("Layout")) {
		if (ImGui::MenuItem("Reset to Default")) {
			rebuildLayoutRequested_ = true;
		}
		ImGui::EndMenu();
	}
	ImGui::Separator();
	const ImVec4 statusColor = viewData.healthy
		? ImVec4{ 0.25f, 0.95f, 0.45f, 1.0f }
		: ImVec4{ 1.0f, 0.25f, 0.20f, 1.0f };
	ImGui::TextColored(
		statusColor,
		viewData.healthy ? "SIMULATION READY" : "SIMULATION STOPPED");
	ImGui::EndMainMenuBar();
#else
	(void)viewData;
	(void)request;
#endif
}

void MagnetPrototypeWindow::BuildDefaultLayout(unsigned int dockspaceId)
{
#ifdef USE_IMGUI
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	if (viewport == nullptr || dockspaceId == 0 ||
		viewport->WorkSize.x <= 1.0f || viewport->WorkSize.y <= 1.0f) {
		return;
	}
	ImGui::DockBuilderRemoveNode(dockspaceId);
	ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
	ImGui::DockBuilderSetNodePos(dockspaceId, viewport->WorkPos);
	ImGui::DockBuilderSetNodeSize(dockspaceId, viewport->WorkSize);

	ImGuiID monitorNode = 0;
	ImGuiID upperNode = 0;
	ImGui::DockBuilderSplitNode(
		dockspaceId,
		ImGuiDir_Down,
		kBottomPanelRatio,
		&monitorNode,
		&upperNode);
	ImGuiID hierarchyNode = 0;
	ImGuiID centerAndInspectorNode = 0;
	ImGui::DockBuilderSplitNode(
		upperNode,
		ImGuiDir_Left,
		kLeftPanelRatio,
		&hierarchyNode,
		&centerAndInspectorNode);
	ImGuiID inspectorNode = 0;
	ImGuiID viewportNode = 0;
	ImGui::DockBuilderSplitNode(
		centerAndInspectorNode,
		ImGuiDir_Right,
		kRightPanelRatio,
		&inspectorNode,
		&viewportNode);
	ImGui::DockBuilderDockWindow("MagnetHierarchy", hierarchyNode);
	ImGui::DockBuilderDockWindow("MagnetViewport", viewportNode);
	ImGui::DockBuilderDockWindow("MagnetInspector", inspectorNode);
	ImGui::DockBuilderDockWindow("MagnetMonitor", monitorNode);
	ImGui::DockBuilderFinish(dockspaceId);
	rebuildLayoutRequested_ = false;
#else
	(void)dockspaceId;
#endif
}

void MagnetPrototypeWindow::DrawHierarchy(const MagnetPrototypeViewData& viewData)
{
#ifdef USE_IMGUI
	if (ImGui::Begin(kHierarchyWindowName, nullptr, ImGuiWindowFlags_NoCollapse)) {
		ImGui::TextDisabled("STAGE OBJECTS");
		ImGui::Separator();
		ImGui::BulletText("Player (Kinematic)");
		const MagnetStageData* stageData = viewData.stageData;
		bool selectedIdExists = selectedStageBallId_ == 0;
		if (stageData && ImGui::TreeNodeEx(
			"Magnet Balls",
			ImGuiTreeNodeFlags_DefaultOpen)) {
			for (std::size_t index = 0; index < stageData->ballCount; ++index) {
				const MagnetStageBallPlacement& ball = stageData->balls[index];
				const bool selected = selectedStageBallId_ == ball.id;
				selectedIdExists = selectedIdExists || selected;
				char label[64]{};
				std::snprintf(label, sizeof(label), "Ball %u###StageBall%u", ball.id, ball.id);
				if (ImGui::Selectable(label, selected)) {
					selectedStageBallId_ = ball.id;
				}
			}
			ImGui::TreePop();
		}
		if (!selectedIdExists) {
			selectedStageBallId_ = 0;
		}
		ImGui::Spacing();
		ImGui::Text("Loose: %zu", viewData.availableBallCount);
		ImGui::Text("Attached: %zu", viewData.attachedBallCount);
		ImGui::Text("Released: %zu", viewData.releasedBallCount);
	}
	ImGui::End();
#else
	(void)viewData;
#endif
}

void MagnetPrototypeWindow::DrawViewport(
	SrvManager* srvManager,
	uint32_t finalDisplaySrvIndex,
	float virtualWidth,
	float virtualHeight)
{
#ifdef USE_IMGUI
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0.0f, 0.0f });
	if (ImGui::Begin(kViewportWindowName, nullptr, ImGuiWindowFlags_NoCollapse)) {
		const bool validTexture = srvManager != nullptr &&
			srvManager->IsAllocated(finalDisplaySrvIndex);
		const bool validVirtualSize = std::isfinite(virtualWidth) &&
			std::isfinite(virtualHeight) &&
			virtualWidth > 1.0f && virtualHeight > 1.0f;
		const ImVec2 contentSize = ImGui::GetContentRegionAvail();
		if (validTexture && validVirtualSize &&
			contentSize.x > 1.0f && contentSize.y > 1.0f) {
			const float targetAspect = virtualWidth / virtualHeight;
			ImVec2 displaySize = contentSize;
			if (displaySize.x / displaySize.y > targetAspect) {
				displaySize.x = displaySize.y * targetAspect;
			} else {
				displaySize.y = displaySize.x / targetAspect;
			}
			const ImVec2 cursorPosition = ImGui::GetCursorPos();
			ImGui::SetCursorPos({
				cursorPosition.x + (contentSize.x - displaySize.x) * 0.5f,
				cursorPosition.y + (contentSize.y - displaySize.y) * 0.5f,
			});
			ImGui::Image(
				static_cast<ImTextureID>(
					srvManager->GetGPUDescriptorHandle(finalDisplaySrvIndex).ptr),
				displaySize);
		} else {
			ImGui::SetCursorPos({ 16.0f, 16.0f });
			ImGui::TextDisabled("Game View is unavailable.");
		}
	}
	ImGui::End();
	ImGui::PopStyleVar();
#else
	(void)srvManager;
	(void)finalDisplaySrvIndex;
	(void)virtualWidth;
	(void)virtualHeight;
#endif
}

void MagnetPrototypeWindow::DrawInspector(
	const MagnetPrototypeViewData& viewData,
	MagnetPrototypeUiRequest& request)
{
#ifdef USE_IMGUI
	if (ImGui::Begin(kInspectorWindowName, nullptr, ImGuiWindowFlags_NoCollapse)) {
		const ImVec4 statusColor = viewData.healthy
			? ImVec4{ 0.25f, 0.95f, 0.45f, 1.0f }
			: ImVec4{ 1.0f, 0.25f, 0.20f, 1.0f };
		ImGui::TextColored(
			statusColor,
			viewData.healthy ? "SIMULATION READY" : "SIMULATION STOPPED");

		ImGui::SeparatorText("Simulation");
		const float spacing = ImGui::GetStyle().ItemSpacing.x;
		const float availableWidth = (std::max)(ImGui::GetContentRegionAvail().x, 1.0f);
		const bool stackButtons = availableWidth < kMinimumButtonWidth * 2.0f + spacing;
		const float buttonWidth = stackButtons
			? availableWidth
			: (availableWidth - spacing) * 0.5f;
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.65f, 0.12f, 0.10f, 1.0f });
		request.emergencyStop = ImGui::Button("STOP", { buttonWidth, 36.0f });
		ImGui::PopStyleColor();
		if (!stackButtons) {
			ImGui::SameLine();
		}
		request.reset = ImGui::Button("RESET STAGE", { buttonWidth, 36.0f });

		ImGui::SeparatorText("Magnetic Coupling");
		ImGui::Text("Left %zu / %zu", viewData.leftChainCount, MagnetChainSystem::kLinksPerSide);
		ImGui::Text("Right %zu / %zu", viewData.rightChainCount, MagnetChainSystem::kLinksPerSide);
		ImGui::Text("Pickup radius: %.2f", viewData.attachmentRadius);
		if (viewData.attachedBallCount == 0) {
			ImGui::BeginDisabled();
		}
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.88f, 0.38f, 0.08f, 1.0f });
		request.releaseChains = ImGui::Button(
			"MAGNET OFF - RELEASE ATTACHED",
			{ (std::max)(ImGui::GetContentRegionAvail().x, 1.0f), 36.0f });
		ImGui::PopStyleColor();
		if (viewData.attachedBallCount == 0) {
			ImGui::EndDisabled();
		}

		DrawStageEditor(viewData, request);

		ImGui::SeparatorText("Viewport");
		ImGui::Checkbox("Grid", &showGrid_);
		ImGui::Checkbox("Velocity Vectors", &showVelocity_);
		ImGui::Checkbox("Camera Follow", &cameraFollow_);
	}
	ImGui::End();
#else
	(void)viewData;
	(void)request;
#endif
}

void MagnetPrototypeWindow::DrawStageEditor(
	const MagnetPrototypeViewData& viewData,
	MagnetPrototypeUiRequest& request)
{
#ifdef USE_IMGUI
	ImGui::SeparatorText("Stage Authoring");
	const MagnetStageData* stageData = viewData.stageData;
	const MagnetStageBallPlacement* selectedBall = nullptr;
	if (stageData) {
		for (std::size_t index = 0; index < stageData->ballCount; ++index) {
			if (stageData->balls[index].id == selectedStageBallId_) {
				selectedBall = &stageData->balls[index];
				break;
			}
		}
	}
	if (selectedBall) {
		ImGui::Text("Selected Ball ID: %u", selectedBall->id);
		float positionXZ[2] = { selectedBall->position.x, selectedBall->position.z };
		ImGui::SetNextItemWidth(-1.0f);
		if (ImGui::DragFloat2("Position XZ", positionXZ, 0.10f)) {
			request.stageAction = MagnetStageEditorAction::MoveBall;
			request.selectedBallId = selectedBall->id;
			request.editedBallPosition = {
				positionXZ[0],
				selectedBall->position.y,
				positionXZ[1],
			};
		}
		if (ImGui::Button("REMOVE SELECTED", { -1.0f, 0.0f })) {
			request.stageAction = MagnetStageEditorAction::RemoveBall;
			request.selectedBallId = selectedBall->id;
		}
	} else {
		ImGui::TextDisabled("Select a ball in Stage Hierarchy to edit it.");
	}
	if (ImGui::Button("ADD BALL AT (0, 4)", { -1.0f, 0.0f })) {
		request.stageAction = MagnetStageEditorAction::AddBall;
		request.editedBallPosition = { 0.0f, 0.5f, 4.0f };
	}

	if (ImGui::TreeNodeEx("Balanced Random Generator", ImGuiTreeNodeFlags_DefaultOpen)) {
		int ballCount = static_cast<int>(generationSettings_.ballCount);
		if (ImGui::SliderInt(
			"Ball Count",
			&ballCount,
			1,
			static_cast<int>(MagnetStageData::kMaximumBallCount))) {
			generationSettings_.ballCount = static_cast<std::size_t>(ballCount);
		}
		ImGui::InputScalar("Seed", ImGuiDataType_U32, &generationSettings_.seed);
		float xBounds[2] = {
			generationSettings_.minimumX,
			generationSettings_.maximumX,
		};
		float zBounds[2] = {
			generationSettings_.minimumZ,
			generationSettings_.maximumZ,
		};
		if (ImGui::DragFloat2("X Bounds", xBounds, 0.25f)) {
			generationSettings_.minimumX = xBounds[0];
			generationSettings_.maximumX = xBounds[1];
		}
		if (ImGui::DragFloat2("Z Bounds", zBounds, 0.25f)) {
			generationSettings_.minimumZ = zBounds[0];
			generationSettings_.maximumZ = zBounds[1];
		}
		ImGui::DragFloat(
			"Minimum Spacing",
			&generationSettings_.minimumSpacing,
			0.05f,
			0.5f,
			20.0f);
		ImGui::DragFloat(
			"Player Clear Radius",
			&generationSettings_.playerClearRadius,
			0.05f,
			0.0f,
			30.0f);
		if (ImGui::Button("GENERATE BALANCED RANDOM", { -1.0f, 32.0f })) {
			request.stageAction = MagnetStageEditorAction::GenerateBalanced;
			request.generationSettings = generationSettings_;
		}
		ImGui::TreePop();
	}

	DrawStageSaveBrowser(viewData, request);
#else
	(void)viewData;
	(void)request;
#endif
}

void MagnetPrototypeWindow::DrawStageSaveBrowser(
	const MagnetPrototypeViewData& viewData,
	MagnetPrototypeUiRequest& request)
{
#ifdef USE_IMGUI
	ImGui::SeparatorText("Stage Saves");
	ImGui::TextColored(
		viewData.stageDirty
			? ImVec4{ 1.0f, 0.72f, 0.20f, 1.0f }
			: ImVec4{ 0.35f, 0.90f, 0.50f, 1.0f },
		viewData.stageDirty ? "UNSAVED CHANGES" : "SAVED");
	ImGui::SetNextItemWidth(-1.0f);
	ImGui::InputText("Save Name", saveName_.data(), saveName_.size());
	ImGui::TextDisabled("Use A-Z, a-z, 0-9, '_' or '-' (max 48).");

	const float spacing = ImGui::GetStyle().ItemSpacing.x;
	const float availableWidth = (std::max)(ImGui::GetContentRegionAvail().x, 2.0f);
	const float topButtonWidth = (availableWidth - spacing) * 0.65f;
	if (ImGui::Button("SAVE NEW", { topButtonWidth, 30.0f })) {
		pendingSaveName_ = saveName_;
		if (SaveEntryExists(viewData, saveName_.data())) {
			ImGui::OpenPopup("Confirm Overwrite###MagnetOverwriteConfirm");
		} else {
			selectedSaveName_ = saveName_;
			request.stageAction = MagnetStageEditorAction::SaveNamed;
			CopySaveNameToRequest(request, saveName_, false);
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("REFRESH", { availableWidth - topButtonWidth - spacing, 30.0f })) {
		request.stageAction = MagnetStageEditorAction::RefreshSaves;
	}

	ImGui::TextDisabled("SAVED STAGES (%zu)", viewData.saveEntryCount);
	bool selectedEntryExists = selectedSaveName_[0] == '\0';
	if (ImGui::BeginChild(
		"MagnetStageSaveList",
		{ 0.0f, 130.0f },
		true)) {
		if (!viewData.saveEntries || viewData.saveEntryCount == 0) {
			ImGui::TextDisabled("No stage saves found.");
		} else {
			for (std::size_t index = 0; index < viewData.saveEntryCount; ++index) {
				const MagnetStageSaveEntry& entry = viewData.saveEntries[index];
				const bool selected =
					std::strcmp(selectedSaveName_.data(), entry.name.c_str()) == 0;
				selectedEntryExists = selectedEntryExists || selected;
				if (ImGui::Selectable(entry.name.c_str(), selected)) {
					std::snprintf(
						selectedSaveName_.data(),
						selectedSaveName_.size(),
						"%s",
						entry.name.c_str());
					std::snprintf(
						saveName_.data(),
						saveName_.size(),
						"%s",
						entry.name.c_str());
				}
			}
		}
	}
	ImGui::EndChild();
	if (!selectedEntryExists) {
		selectedSaveName_.fill('\0');
	}

	const bool hasSelection = selectedSaveName_[0] != '\0';
	if (!hasSelection) {
		ImGui::BeginDisabled();
	}
	const float actionButtonWidth = (availableWidth - spacing) * 0.5f;
	if (ImGui::Button("LOAD SELECTED", { actionButtonWidth, 30.0f })) {
		pendingSaveName_ = selectedSaveName_;
		if (viewData.stageDirty) {
			ImGui::OpenPopup("Confirm Load###MagnetLoadConfirm");
		} else {
			request.stageAction = MagnetStageEditorAction::LoadNamed;
			CopySaveNameToRequest(request, selectedSaveName_, false);
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("OVERWRITE", { actionButtonWidth, 30.0f })) {
		pendingSaveName_ = selectedSaveName_;
		ImGui::OpenPopup("Confirm Overwrite###MagnetOverwriteConfirm");
	}
	if (!hasSelection) {
		ImGui::EndDisabled();
	}

	if (ImGui::BeginPopupModal(
		"Confirm Load###MagnetLoadConfirm",
		nullptr,
		ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::Text("Load '%s'?", pendingSaveName_.data());
		ImGui::TextWrapped("Unsaved stage edits will be discarded.");
		if (ImGui::Button("LOAD AND DISCARD", { 160.0f, 0.0f })) {
			request.stageAction = MagnetStageEditorAction::LoadNamed;
			CopySaveNameToRequest(request, pendingSaveName_, false);
			pendingSaveName_.fill('\0');
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("CANCEL", { 90.0f, 0.0f })) {
			pendingSaveName_.fill('\0');
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	if (ImGui::BeginPopupModal(
		"Confirm Overwrite###MagnetOverwriteConfirm",
		nullptr,
		ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::Text("Overwrite '%s'?", pendingSaveName_.data());
		ImGui::TextWrapped("The existing JSON stage will be replaced.");
		if (ImGui::Button("OVERWRITE", { 120.0f, 0.0f })) {
			selectedSaveName_ = pendingSaveName_;
			request.stageAction = MagnetStageEditorAction::SaveNamed;
			CopySaveNameToRequest(request, pendingSaveName_, true);
			pendingSaveName_.fill('\0');
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("CANCEL", { 90.0f, 0.0f })) {
			pendingSaveName_.fill('\0');
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	const ImVec4 operationColor = viewData.stageOperationSucceeded
		? ImVec4{ 0.25f, 0.95f, 0.45f, 1.0f }
		: ImVec4{ 1.0f, 0.45f, 0.20f, 1.0f };
	ImGui::PushStyleColor(ImGuiCol_Text, operationColor);
	ImGui::TextWrapped(
		"%s",
		viewData.stageOperationMessage ? viewData.stageOperationMessage : "");
	ImGui::PopStyleColor();
#else
	(void)viewData;
	(void)request;
#endif
}

void MagnetPrototypeWindow::DrawMonitor(const MagnetPrototypeViewData& viewData)
{
#ifdef USE_IMGUI
	if (ImGui::Begin(kMonitorWindowName, nullptr, ImGuiWindowFlags_NoCollapse)) {
		if (ImGui::BeginTable(
			"MagnetMetrics",
			5,
			ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchSame)) {
			ImGui::TableNextColumn();
			ImGui::TextDisabled("BODIES");
			ImGui::Text("%zu", viewData.bodyCount);
			ImGui::TableNextColumn();
			ImGui::TextDisabled("CONSTRAINTS");
			ImGui::Text("%zu / %zu", viewData.activeConstraintCount, viewData.constraintCount);
			ImGui::TableNextColumn();
			ImGui::TextDisabled("PLAYER SPEED");
			ImGui::Text("%.2f", viewData.playerSpeed);
			ImGui::TableNextColumn();
			ImGui::TextDisabled("LOOSE / ATTACHED");
			ImGui::Text("%zu / %zu", viewData.availableBallCount, viewData.attachedBallCount);
			ImGui::TableNextColumn();
			ImGui::TextDisabled("RELEASED");
			ImGui::Text("%zu", viewData.releasedBallCount);
			ImGui::EndTable();
		}

		const bool finiteConstraintError =
			std::isfinite(viewData.maximumConstraintError) &&
			viewData.maximumConstraintError >= 0.0f;
		ImGui::Text(
			"Maximum constraint error: %s",
			finiteConstraintError ? "finite" : "NON-FINITE");
		const float normalizedError = finiteConstraintError
			? std::clamp(
				viewData.maximumConstraintError / kConstraintWarningError,
				0.0f,
				1.0f)
			: 1.0f;
		ImGui::ProgressBar(normalizedError, { -1.0f, 0.0f }, "constraint stability");
		ImGui::TextDisabled(
			"WASD Move  |  Space Stop  |  Q Release  |  R Reset Stage");
	}
	ImGui::End();
#else
	(void)viewData;
#endif
}

void MagnetPrototypeWindow::CopySaveNameToRequest(
	MagnetPrototypeUiRequest& request,
	const std::array<char, MagnetStageSystem::kMaximumSaveNameLength + 1>& saveName,
	bool allowOverwrite) const noexcept
{
	request.stageSaveName = saveName;
	request.allowOverwrite = allowOverwrite;
}

bool MagnetPrototypeWindow::SaveEntryExists(
	const MagnetPrototypeViewData& viewData,
	const char* saveName) const noexcept
{
	if (!viewData.saveEntries || !saveName) {
		return false;
	}
	for (std::size_t index = 0; index < viewData.saveEntryCount; ++index) {
		const std::string& entryName = viewData.saveEntries[index].name;
		std::size_t characterIndex = 0;
		while (characterIndex < entryName.size() && saveName[characterIndex] != '\0') {
			const char entryCharacter = entryName[characterIndex];
			const char inputCharacter = saveName[characterIndex];
			const char lowerEntry = entryCharacter >= 'A' && entryCharacter <= 'Z'
				? static_cast<char>(entryCharacter - 'A' + 'a')
				: entryCharacter;
			const char lowerInput = inputCharacter >= 'A' && inputCharacter <= 'Z'
				? static_cast<char>(inputCharacter - 'A' + 'a')
				: inputCharacter;
			if (lowerEntry != lowerInput) {
				break;
			}
			++characterIndex;
		}
		if (characterIndex == entryName.size() && saveName[characterIndex] == '\0') {
			return true;
		}
	}
	return false;
}

} // namespace magnet
