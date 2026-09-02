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
constexpr char kHierarchyWindowName[] = "ステージ階層###MagnetHierarchy";
constexpr char kViewportWindowName[] = "ゲーム画面###MagnetViewport";
constexpr char kInspectorWindowName[] = "ステージ設定###MagnetInspector";
constexpr char kMonitorWindowName[] = "シミュレーション監視###MagnetMonitor";
constexpr float kLeftPanelRatio = 0.20f;
constexpr float kRightPanelRatio = 0.28f;
constexpr float kBottomPanelRatio = 0.22f;
constexpr float kMinimumButtonWidth = 80.0f;
constexpr float kConstraintWarningError = 0.08f;
constexpr float kMinimapSize = 150.0f;
constexpr float kMinimapMargin = 14.0f;
constexpr float kMinimapWorldRadius = 24.0f;

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

	DrawHierarchy(viewData, request);
	DrawViewport(
		viewData,
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
	request.spinChargeSettings = spinChargeSettings_;
	request.impactAttachmentSettings = impactAttachmentSettings_;
	request.showVelocity = showVelocity_;
	request.cameraFollow = cameraFollow_;
	request.selectedObjectType = selectedObjectType_;
	request.selectedObjectId = selectedObjectId_;
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
	ImGui::TextUnformatted("磁石ステージエディター");
	ImGui::Separator();
	if (ImGui::BeginMenu("ステージ")) {
		if (ImGui::MenuItem("セーブ一覧を更新")) {
			request.stageAction = MagnetStageEditorAction::RefreshSaves;
		}
		ImGui::Separator();
		if (ImGui::MenuItem(
			"偏りを抑えてランダム配置",
			nullptr,
			false,
			viewData.editorMode == MagnetEditorMode::StageEdit)) {
			request.stageAction = MagnetStageEditorAction::GenerateBalanced;
			request.generationSettings = generationSettings_;
		}
		ImGui::EndMenu();
	}
	if (ImGui::BeginMenu("画面配置")) {
		if (ImGui::MenuItem("初期配置へ戻す")) {
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
		viewData.healthy ? "シミュレーション正常" : "シミュレーション停止");
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

void MagnetPrototypeWindow::DrawHierarchy(
	const MagnetPrototypeViewData& viewData,
	MagnetPrototypeUiRequest& request)
{
#ifdef USE_IMGUI
	if (ImGui::Begin(kHierarchyWindowName, nullptr, ImGuiWindowFlags_NoCollapse)) {
		ImGui::TextDisabled("ステージオブジェクト");
		ImGui::Separator();
		bool selectedObjectExists = selectedObjectType_ == MagnetStageObjectType::None;
		const bool playerSelected = selectedObjectType_ == MagnetStageObjectType::Player;
		selectedObjectExists = selectedObjectExists || playerSelected;
		if (ImGui::Selectable("プレイヤー###StagePlayer", playerSelected)) {
			selectedObjectType_ = MagnetStageObjectType::Player;
			selectedObjectId_ = 0;
			selectedObjectExists = true;
		}
		const MagnetStageData* stageData = viewData.stageData;
		if (stageData && ImGui::TreeNodeEx(
			"小さい球",
			ImGuiTreeNodeFlags_DefaultOpen)) {
			for (std::size_t index = 0; index < stageData->ballCount; ++index) {
				const MagnetStageBallPlacement& ball = stageData->balls[index];
				const bool selected = selectedObjectType_ == MagnetStageObjectType::MagnetBall &&
					selectedObjectId_ == ball.id;
				selectedObjectExists = selectedObjectExists || selected;
				char label[64]{};
				std::snprintf(label, sizeof(label), "球 %u###StageBall%u", ball.id, ball.id);
				if (ImGui::Selectable(label, selected)) {
					selectedObjectType_ = MagnetStageObjectType::MagnetBall;
					selectedObjectId_ = ball.id;
					selectedObjectExists = true;
				}
			}
			ImGui::TreePop();
		}
		if (stageData && ImGui::TreeNodeEx("ゴール", ImGuiTreeNodeFlags_DefaultOpen)) {
			for (std::size_t index = 0; index < stageData->goalCount; ++index) {
				const MagnetStageBoxPlacement& goal = stageData->goals[index];
				const bool selected = selectedObjectType_ == MagnetStageObjectType::Goal &&
					selectedObjectId_ == goal.id;
				selectedObjectExists = selectedObjectExists || selected;
				char label[64]{};
				std::snprintf(label, sizeof(label), "ゴール %u###StageGoal%u", goal.id, goal.id);
				if (ImGui::Selectable(label, selected)) {
					selectedObjectType_ = MagnetStageObjectType::Goal;
					selectedObjectId_ = goal.id;
					selectedObjectExists = true;
				}
			}
			ImGui::TreePop();
		}
		if (stageData && ImGui::TreeNodeEx("障害物", ImGuiTreeNodeFlags_DefaultOpen)) {
			for (std::size_t index = 0; index < stageData->obstacleCount; ++index) {
				const MagnetStageBoxPlacement& obstacle = stageData->obstacles[index];
				const bool selected = selectedObjectType_ == MagnetStageObjectType::Obstacle &&
					selectedObjectId_ == obstacle.id;
				selectedObjectExists = selectedObjectExists || selected;
				char label[64]{};
				std::snprintf(
					label,
					sizeof(label),
					"障害物 %u###StageObstacle%u",
					obstacle.id,
					obstacle.id);
				if (ImGui::Selectable(label, selected)) {
					selectedObjectType_ = MagnetStageObjectType::Obstacle;
					selectedObjectId_ = obstacle.id;
					selectedObjectExists = true;
				}
			}
			ImGui::TreePop();
		}
		if (!selectedObjectExists) {
			selectedObjectType_ = MagnetStageObjectType::None;
			selectedObjectId_ = 0;
		}
		ImGui::Spacing();
		ImGui::Text("未吸着: %zu", viewData.availableBallCount);
		ImGui::Text("吸着中: %zu", viewData.attachedBallCount);
		ImGui::Text("射出済み: %zu", viewData.releasedBallCount);
	}
	ImGui::End();
#else
	(void)viewData;
	(void)request;
#endif
}

void MagnetPrototypeWindow::DrawViewport(
	const MagnetPrototypeViewData& viewData,
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
			const ImVec2 imagePosition = ImGui::GetCursorScreenPos();
			ImGui::Image(
				static_cast<ImTextureID>(
					srvManager->GetGPUDescriptorHandle(finalDisplaySrvIndex).ptr),
				displaySize);

			const float minimapSize = (std::min)(kMinimapSize, displaySize.y * 0.28f);
			const ImVec2 mapMinimum = {
				imagePosition.x + displaySize.x - minimapSize - kMinimapMargin,
				imagePosition.y + kMinimapMargin,
			};
			const ImVec2 mapMaximum = { mapMinimum.x + minimapSize, mapMinimum.y + minimapSize };
			const ImVec2 mapCenter = {
				(mapMinimum.x + mapMaximum.x) * 0.5f,
				(mapMinimum.y + mapMaximum.y) * 0.5f,
			};
			ImDrawList* drawList = ImGui::GetWindowDrawList();
			drawList->AddRectFilled(mapMinimum, mapMaximum, IM_COL32(10, 16, 24, 205), 7.0f);
			drawList->AddRect(mapMinimum, mapMaximum, IM_COL32(210, 225, 240, 230), 7.0f, 0, 2.0f);
			drawList->AddLine({ mapCenter.x, mapMinimum.y + 6.0f },
				{ mapCenter.x, mapMaximum.y - 6.0f }, IM_COL32(90, 105, 120, 150));
			drawList->AddLine({ mapMinimum.x + 6.0f, mapCenter.y },
				{ mapMaximum.x - 6.0f, mapCenter.y }, IM_COL32(90, 105, 120, 150));
			drawList->AddCircleFilled(mapCenter, 4.0f, IM_COL32(90, 210, 255, 255));
			const float usableRadius = minimapSize * 0.5f - 9.0f;
			for (std::size_t index = 0; index < viewData.offscreenMagnetCount; ++index) {
				Vector2 offset = viewData.offscreenMagnetOffsets[index];
				float normalizedX = offset.x / kMinimapWorldRadius;
				float normalizedZ = offset.y / kMinimapWorldRadius;
				const float maximumComponent = (std::max)(std::abs(normalizedX), std::abs(normalizedZ));
				if (maximumComponent > 1.0f) {
					normalizedX /= maximumComponent;
					normalizedZ /= maximumComponent;
				}
				const ImVec2 marker = {
					mapCenter.x + normalizedX * usableRadius,
					mapCenter.y - normalizedZ * usableRadius,
				};
				drawList->AddCircleFilled(marker, 4.5f, IM_COL32(245, 55, 55, 255));
				drawList->AddCircle(marker, 4.5f, IM_COL32(255, 205, 205, 255), 0, 1.5f);
			}
			drawList->AddText({ mapMinimum.x + 7.0f, mapMinimum.y + 5.0f },
				IM_COL32(235, 240, 245, 230), "MINIMAP");
		} else {
			ImGui::SetCursorPos({ 16.0f, 16.0f });
			ImGui::TextDisabled("ゲーム画面を表示できません。");
		}
	}
	ImGui::End();
	ImGui::PopStyleVar();
#else
	(void)viewData;
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
			viewData.healthy ? "シミュレーション正常" : "シミュレーション停止");

		ImGui::SeparatorText("デバッグモード");
		const float modeSpacing = ImGui::GetStyle().ItemSpacing.x;
		const float modeWidth =
			((std::max)(ImGui::GetContentRegionAvail().x, 2.0f) - modeSpacing) * 0.5f;
		if (viewData.editorMode == MagnetEditorMode::Play) {
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.15f, 0.50f, 0.85f, 1.0f });
		}
		if (ImGui::Button("プレイ", { modeWidth, 34.0f }) &&
			viewData.editorMode != MagnetEditorMode::Play) {
			request.modeChangeRequested = true;
			request.requestedMode = MagnetEditorMode::Play;
		}
		if (viewData.editorMode == MagnetEditorMode::Play) {
			ImGui::PopStyleColor();
		}
		ImGui::SameLine();
		if (viewData.editorMode == MagnetEditorMode::StageEdit) {
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.20f, 0.65f, 0.32f, 1.0f });
		}
		if (ImGui::Button("ステージ配置", { modeWidth, 34.0f }) &&
			viewData.editorMode != MagnetEditorMode::StageEdit) {
			request.modeChangeRequested = true;
			request.requestedMode = MagnetEditorMode::StageEdit;
		}
		if (viewData.editorMode == MagnetEditorMode::StageEdit) {
			ImGui::PopStyleColor();
		}

		ImGui::SeparatorText("シミュレーション");
		if (viewData.editorMode == MagnetEditorMode::StageEdit) {
			ImGui::BeginDisabled();
		}
		const float spacing = ImGui::GetStyle().ItemSpacing.x;
		const float availableWidth = (std::max)(ImGui::GetContentRegionAvail().x, 1.0f);
		const bool stackButtons = availableWidth < kMinimumButtonWidth * 2.0f + spacing;
		const float buttonWidth = stackButtons
			? availableWidth
			: (availableWidth - spacing) * 0.5f;
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.65f, 0.12f, 0.10f, 1.0f });
		request.emergencyStop = ImGui::Button("急停止", { buttonWidth, 36.0f });
		ImGui::PopStyleColor();
		if (!stackButtons) {
			ImGui::SameLine();
		}
		request.reset = ImGui::Button("ステージをリセット", { buttonWidth, 36.0f });

		ImGui::SeparatorText("磁力接続");
		ImGui::Text("左 %zu / %zu", viewData.leftChainCount, MagnetChainSystem::kLinksPerSide);
		ImGui::Text("右 %zu / %zu", viewData.rightChainCount, MagnetChainSystem::kLinksPerSide);
		ImGui::Text("吸着半径: %.2f", viewData.attachmentRadius);
		if (viewData.attachedBallCount == 0) {
			ImGui::BeginDisabled();
		}
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.88f, 0.38f, 0.08f, 1.0f });
		request.releaseChains = ImGui::Button(
			"磁力OFF - 吸着中の球を射出",
			{ (std::max)(ImGui::GetContentRegionAvail().x, 1.0f), 36.0f });
		ImGui::PopStyleColor();
		if (viewData.attachedBallCount == 0) {
			ImGui::EndDisabled();
		}
		ImGui::SeparatorText("回転チャージ射出");
		ImGui::Checkbox("回転チャージを有効化", &spinChargeSettings_.enabled);
		ImGui::SliderFloat(
			"最大チャージまでの回転数",
			&spinChargeSettings_.rotationsForFullCharge,
			0.25f,
			12.0f,
			"%.2f 回");
		ImGui::SliderFloat(
			"最大旋回倍率",
			&spinChargeSettings_.maximumTurnSpeedMultiplier,
			1.0f,
			12.0f,
			"x%.1f");
		ImGui::SliderFloat(
			"最大射出倍率",
			&spinChargeSettings_.maximumSpeedMultiplier,
			1.0f,
			16.0f,
			"x%.1f");
		ImGui::ProgressBar(viewData.spinChargeRatio, { -1.0f, 0.0f }, "チャージ量");
		ImGui::Text(
			"蓄積 %.2f回 | 旋回 x%.2f | 射出 x%.2f",
			viewData.spinChargeRotations,
			viewData.spinChargeTurnSpeedMultiplier,
			viewData.spinChargeSpeedMultiplier);
		ImGui::SeparatorText("射出球どうしの磁力接続");
		ImGui::Checkbox("衝突した磁石球を接続", &impactAttachmentSettings_.enabled);
		ImGui::SliderFloat(
			"接続に必要な最低衝突速度",
			&impactAttachmentSettings_.minimumImpactSpeed,
			0.0f,
			20.0f,
			"%.1f");
		ImGui::Text("接続中の組: %zu", viewData.magneticAttachmentCount);
		if (viewData.editorMode == MagnetEditorMode::StageEdit) {
			ImGui::EndDisabled();
		}

		if (viewData.editorMode == MagnetEditorMode::StageEdit) {
			DrawStageEditor(viewData, request);
		} else {
			ImGui::SeparatorText("ステージ配置");
			ImGui::TextWrapped("配置を編集するには「ステージ配置」モードへ切り替えてください。");
		}

		ImGui::SeparatorText("表示");
		ImGui::Checkbox("グリッド", &showGrid_);
		ImGui::Checkbox("速度ベクトル", &showVelocity_);
		ImGui::Checkbox("カメラ追従", &cameraFollow_);
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
	ImGui::SeparatorText("ステージ配置");
	const MagnetStageData* stageData = viewData.stageData;
	const MagnetStageBallPlacement* selectedBall = nullptr;
	const MagnetStageBoxPlacement* selectedBox = nullptr;
	if (stageData) {
		if (selectedObjectType_ == MagnetStageObjectType::MagnetBall) {
			for (std::size_t index = 0; index < stageData->ballCount; ++index) {
				if (stageData->balls[index].id == selectedObjectId_) {
					selectedBall = &stageData->balls[index];
					break;
				}
			}
		} else if (selectedObjectType_ == MagnetStageObjectType::Goal) {
			for (std::size_t index = 0; index < stageData->goalCount; ++index) {
				if (stageData->goals[index].id == selectedObjectId_) {
					selectedBox = &stageData->goals[index];
					break;
				}
			}
		} else if (selectedObjectType_ == MagnetStageObjectType::Obstacle) {
			for (std::size_t index = 0; index < stageData->obstacleCount; ++index) {
				if (stageData->obstacles[index].id == selectedObjectId_) {
					selectedBox = &stageData->obstacles[index];
					break;
				}
			}
		}
	}
	if (stageData && selectedObjectType_ == MagnetStageObjectType::Player) {
		ImGui::TextColored(
			ImVec4(1.0f, 0.35f, 0.35f, 1.0f),
			"プレイヤー開始位置");
		ImGui::TextDisabled("地面上を移動するため高さは固定です。");
		float positionXZ[2] = {
			stageData->playerPosition.x,
			stageData->playerPosition.z,
		};
		ImGui::SetNextItemWidth(-1.0f);
		if (ImGui::DragFloat2("位置 XZ##PlayerPosition", positionXZ, 0.10f)) {
			request.stageAction = MagnetStageEditorAction::MovePlayer;
			request.editedObjectPosition = {
				positionXZ[0],
				stageData->playerPosition.y,
				positionXZ[1],
			};
		}
	} else if (selectedBall) {
		ImGui::TextColored(
			ImVec4(1.0f, 0.82f, 0.20f, 1.0f),
			"小さい球 %u",
			selectedBall->id);
		ImGui::TextDisabled("吸着前の配置です。高さは地面に固定されます。");
		float positionXZ[2] = { selectedBall->position.x, selectedBall->position.z };
		ImGui::SetNextItemWidth(-1.0f);
		if (ImGui::DragFloat2("位置 XZ##BallPosition", positionXZ, 0.10f)) {
			request.stageAction = MagnetStageEditorAction::MoveBall;
			request.selectedBallId = selectedBall->id;
			request.editedObjectPosition = {
				positionXZ[0],
				selectedBall->position.y,
				positionXZ[1],
			};
		}
		if (ImGui::Button("選択した球を削除", { -1.0f, 0.0f })) {
			request.stageAction = MagnetStageEditorAction::RemoveBall;
			request.selectedBallId = selectedBall->id;
		}
	} else if (selectedBox) {
		const bool isGoal = selectedObjectType_ == MagnetStageObjectType::Goal;
		ImGui::TextColored(
			isGoal
				? ImVec4(0.25f, 1.0f, 0.45f, 1.0f)
				: ImVec4(1.0f, 0.55f, 0.20f, 1.0f),
			isGoal ? "ゴール %u" : "障害物 %u",
			selectedBox->id);
		ImGui::TextDisabled(
			isGoal
				? "位置と得点判定エリアの大きさを編集します。"
				: "位置と配置用ボックスの大きさを編集します。");
		float position[3] = {
			selectedBox->position.x,
			selectedBox->position.y,
			selectedBox->position.z,
		};
		float size[3] = {
			selectedBox->size.x,
			selectedBox->size.y,
			selectedBox->size.z,
		};
		const char* positionLabel = isGoal
			? "位置 XYZ##GoalPosition"
			: "位置 XYZ##ObstaclePosition";
		const char* sizeLabel = isGoal
			? "得点エリア XYZ##GoalSize"
			: "サイズ XYZ##ObstacleSize";
		bool transformChanged = ImGui::DragFloat3(positionLabel, position, 0.10f);
		transformChanged = ImGui::DragFloat3(sizeLabel, size, 0.10f, 0.10f, 50.0f) ||
			transformChanged;
		if (transformChanged) {
			request.stageAction = MagnetStageEditorAction::MoveBoxObject;
			request.selectedObjectType = selectedObjectType_;
			request.selectedObjectId = selectedBox->id;
			request.editedObjectPosition = { position[0], position[1], position[2] };
			request.editedObjectSize = { size[0], size[1], size[2] };
		}
		if (ImGui::Button(
			isGoal ? "選択したゴールを削除" : "選択した障害物を削除",
			{ -1.0f, 0.0f })) {
			request.stageAction = MagnetStageEditorAction::RemoveBoxObject;
			request.selectedObjectType = selectedObjectType_;
			request.selectedObjectId = selectedBox->id;
		}
	} else {
		ImGui::TextDisabled("左の一覧からオブジェクトを選択してください。");
	}
	if (ImGui::Button("球を追加", { -1.0f, 0.0f })) {
		request.stageAction = MagnetStageEditorAction::AddBall;
		request.editedObjectPosition = { 0.0f, 0.5f, 4.0f };
	}
	if (ImGui::Button("ゴールを追加", { -1.0f, 0.0f })) {
		request.stageAction = MagnetStageEditorAction::AddGoal;
		request.editedObjectPosition = { 0.0f, 1.0f, 8.0f };
		request.editedObjectSize = { 2.5f, 2.0f, 1.0f };
	}
	if (ImGui::Button("障害物を追加", { -1.0f, 0.0f })) {
		request.stageAction = MagnetStageEditorAction::AddObstacle;
		request.editedObjectPosition = { 0.0f, 1.0f, 4.0f };
		request.editedObjectSize = { 2.0f, 2.0f, 2.0f };
	}

	if (ImGui::TreeNodeEx("球のランダム配置", ImGuiTreeNodeFlags_DefaultOpen)) {
		int ballCount = static_cast<int>(generationSettings_.ballCount);
		if (ImGui::SliderInt(
			"球の数",
			&ballCount,
			1,
			static_cast<int>(MagnetStageData::kMaximumBallCount))) {
			generationSettings_.ballCount = static_cast<std::size_t>(ballCount);
		}
		ImGui::InputScalar("乱数シード", ImGuiDataType_U32, &generationSettings_.seed);
		float xBounds[2] = {
			generationSettings_.minimumX,
			generationSettings_.maximumX,
		};
		float zBounds[2] = {
			generationSettings_.minimumZ,
			generationSettings_.maximumZ,
		};
		if (ImGui::DragFloat2("X範囲", xBounds, 0.25f)) {
			generationSettings_.minimumX = xBounds[0];
			generationSettings_.maximumX = xBounds[1];
		}
		if (ImGui::DragFloat2("Z範囲", zBounds, 0.25f)) {
			generationSettings_.minimumZ = zBounds[0];
			generationSettings_.maximumZ = zBounds[1];
		}
		ImGui::DragFloat(
			"球同士の最小間隔",
			&generationSettings_.minimumSpacing,
			0.05f,
			0.5f,
			20.0f);
		ImGui::DragFloat(
			"プレイヤー周辺の空白",
			&generationSettings_.playerClearRadius,
			0.05f,
			0.0f,
			30.0f);
		if (ImGui::Button("偏りを抑えてランダム配置", { -1.0f, 32.0f })) {
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
	ImGui::SeparatorText("ステージのセーブ・ロード");
	ImGui::TextColored(
		viewData.stageDirty
			? ImVec4{ 1.0f, 0.72f, 0.20f, 1.0f }
			: ImVec4{ 0.35f, 0.90f, 0.50f, 1.0f },
		viewData.stageDirty ? "未保存の変更あり" : "保存済み");
	ImGui::SetNextItemWidth(-1.0f);
	ImGui::InputText("セーブ名", saveName_.data(), saveName_.size());
	ImGui::TextDisabled("英数字・_・- を使用（48文字以内）");

	const float spacing = ImGui::GetStyle().ItemSpacing.x;
	const float availableWidth = (std::max)(ImGui::GetContentRegionAvail().x, 2.0f);
	const float topButtonWidth = (availableWidth - spacing) * 0.65f;
	if (ImGui::Button("新規セーブ", { topButtonWidth, 30.0f })) {
		pendingSaveName_ = saveName_;
		if (SaveEntryExists(viewData, saveName_.data())) {
			ImGui::OpenPopup("上書きの確認###MagnetOverwriteConfirm");
		} else {
			selectedSaveName_ = saveName_;
			request.stageAction = MagnetStageEditorAction::SaveNamed;
			CopySaveNameToRequest(request, saveName_, false);
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("更新", { availableWidth - topButtonWidth - spacing, 30.0f })) {
		request.stageAction = MagnetStageEditorAction::RefreshSaves;
	}

	ImGui::TextDisabled("セーブ済みステージ (%zu)", viewData.saveEntryCount);
	bool selectedEntryExists = selectedSaveName_[0] == '\0';
	if (ImGui::BeginChild(
		"MagnetStageSaveList",
		{ 0.0f, 130.0f },
		true)) {
		if (!viewData.saveEntries || viewData.saveEntryCount == 0) {
			ImGui::TextDisabled("セーブデータがありません。");
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
	if (ImGui::Button("選択データをロード", { actionButtonWidth, 30.0f })) {
		pendingSaveName_ = selectedSaveName_;
		if (viewData.stageDirty) {
			ImGui::OpenPopup("ロードの確認###MagnetLoadConfirm");
		} else {
			request.stageAction = MagnetStageEditorAction::LoadNamed;
			CopySaveNameToRequest(request, selectedSaveName_, false);
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("上書き", { actionButtonWidth, 30.0f })) {
		pendingSaveName_ = selectedSaveName_;
		ImGui::OpenPopup("上書きの確認###MagnetOverwriteConfirm");
	}
	if (!hasSelection) {
		ImGui::EndDisabled();
	}

	if (ImGui::BeginPopupModal(
		"ロードの確認###MagnetLoadConfirm",
		nullptr,
		ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::Text("「%s」をロードしますか？", pendingSaveName_.data());
		ImGui::TextWrapped("未保存のステージ編集は破棄されます。");
		if (ImGui::Button("破棄してロード", { 160.0f, 0.0f })) {
			request.stageAction = MagnetStageEditorAction::LoadNamed;
			CopySaveNameToRequest(request, pendingSaveName_, false);
			pendingSaveName_.fill('\0');
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("キャンセル", { 90.0f, 0.0f })) {
			pendingSaveName_.fill('\0');
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	if (ImGui::BeginPopupModal(
		"上書きの確認###MagnetOverwriteConfirm",
		nullptr,
		ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::Text("「%s」を上書きしますか？", pendingSaveName_.data());
		ImGui::TextWrapped("既存のステージJSONが置き換わります。");
		if (ImGui::Button("上書きする", { 120.0f, 0.0f })) {
			selectedSaveName_ = pendingSaveName_;
			request.stageAction = MagnetStageEditorAction::SaveNamed;
			CopySaveNameToRequest(request, pendingSaveName_, true);
			pendingSaveName_.fill('\0');
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("キャンセル", { 90.0f, 0.0f })) {
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
			6,
			ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchSame)) {
			ImGui::TableNextColumn();
			ImGui::TextDisabled("物理ボディ");
			ImGui::Text("%zu", viewData.bodyCount);
			ImGui::TableNextColumn();
			ImGui::TextDisabled("接続制約");
			ImGui::Text("%zu / %zu", viewData.activeConstraintCount, viewData.constraintCount);
			ImGui::TableNextColumn();
			ImGui::TextDisabled("プレイヤー速度");
			ImGui::Text("%.2f", viewData.playerSpeed);
			ImGui::TableNextColumn();
			ImGui::TextDisabled("未吸着 / 吸着中");
			ImGui::Text("%zu / %zu", viewData.availableBallCount, viewData.attachedBallCount);
			ImGui::TableNextColumn();
			ImGui::TextDisabled("射出済み");
			ImGui::Text("%zu", viewData.releasedBallCount);
			ImGui::TableNextColumn();
			ImGui::TextDisabled("ゴール得点");
			ImGui::Text("%zu", viewData.goalHitCount);
			ImGui::EndTable();
		}

		const bool finiteConstraintError =
			std::isfinite(viewData.maximumConstraintError) &&
			viewData.maximumConstraintError >= 0.0f;
		ImGui::Text(
			"最大制約誤差: %s",
			finiteConstraintError ? "正常" : "不正値");
		const float normalizedError = finiteConstraintError
			? std::clamp(
				viewData.maximumConstraintError / kConstraintWarningError,
				0.0f,
				1.0f)
			: 1.0f;
		ImGui::ProgressBar(normalizedError, { -1.0f, 0.0f }, "接続の安定度");
		ImGui::TextDisabled(
			"WASD 移動  |  Space 急停止  |  Q 射出  |  R リセット");
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
