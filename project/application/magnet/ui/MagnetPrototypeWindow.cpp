#include "application/magnet/ui/MagnetPrototypeWindow.h"

#include "base/SrvManager.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui_internal.h"
#endif

#include <algorithm>
#include <climits>
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
constexpr float kMinimapMargin = 14.0f;
constexpr int kObstacleKindCount =
	static_cast<int>(MagnetObstacleKind::Count);

const char* GetObstacleKindName(MagnetObstacleKind kind) noexcept
{
	switch (kind) {
	case MagnetObstacleKind::Solid: return "通常の壁";
	case MagnetObstacleKind::Chainsaw: return "チェーンソー";
	case MagnetObstacleKind::PinballBumper: return "ピンボールバンパー";
	case MagnetObstacleKind::Furnace: return "溶鉱炉";
	case MagnetObstacleKind::MagneticAnchor: return "磁石アンカー";
	case MagnetObstacleKind::TimedShutter: return "開閉シャッター";
	case MagnetObstacleKind::TransferGate: return "転送ゲート";
	case MagnetObstacleKind::RepulsionField: return "反発磁場";
	default: return "不明";
	}
}

const char* GetObstacleKindDescription(MagnetObstacleKind kind) noexcept
{
	switch (kind) {
	case MagnetObstacleKind::Solid:
		return "プレイヤーと球を通さない基本的な壁です。";
	case MagnetObstacleKind::Chainsaw:
		return "吸着中の球に触れると、その球から外側の列を切り離します。";
	case MagnetObstacleKind::PinballBumper:
		return "円形の当たり判定で、触れたプレイヤーや球を強く反射します。";
	case MagnetObstacleKind::Furnace:
		return "触れた小さい球を消滅させます。列の途中なら外側だけが残って飛びます。";
	case MagnetObstacleKind::MagneticAnchor:
		return "近い小さい球を一つ引き寄せ、短時間固定してから解放します。";
	case MagnetObstacleKind::TimedShutter:
		return "一定時間ごとに、通れない状態と通れる状態を切り替えます。";
	case MagnetObstacleKind::TransferGate:
		return "同じペア番号の2個を接続します。プレイヤーと射出済みの球だけを転送し、細いXZ軸が出入口方向になります。プレイヤー転送時は吸着中の球を入口で磁力OFFにします。";
	case MagnetObstacleKind::RepulsionField:
		return "範囲内の小さい球だけを中心から外へ押します。左右に吸着中の球にも効きますが、プレイヤー本体と磁力接続はそのままです。";
	default:
		return "未対応の障害物です。";
	}
}

Vector3 GetObstacleDefaultSize(MagnetObstacleKind kind) noexcept
{
	switch (kind) {
	case MagnetObstacleKind::Chainsaw: return { 3.0f, 1.0f, 0.65f };
	case MagnetObstacleKind::PinballBumper: return { 1.8f, 1.4f, 1.8f };
	case MagnetObstacleKind::Furnace: return { 2.4f, 1.0f, 2.4f };
	case MagnetObstacleKind::MagneticAnchor: return { 2.2f, 1.4f, 2.2f };
	case MagnetObstacleKind::TimedShutter: return { 4.0f, 2.0f, 0.55f };
	case MagnetObstacleKind::TransferGate: return { 0.55f, 2.5f, 4.0f };
	case MagnetObstacleKind::RepulsionField: return { 10.0f, 2.0f, 10.0f };
	case MagnetObstacleKind::Solid:
	default:
		return { 2.0f, 2.0f, 2.0f };
	}
}

uint32_t GetNextBoxId(
	const MagnetStageBoxPlacement* placements,
	std::size_t count) noexcept
{
	uint32_t maximumId = 0;
	for (std::size_t index = 0; index < count; ++index) {
		maximumId = (std::max)(maximumId, placements[index].id);
	}
	return maximumId == UINT32_MAX ? 0u : maximumId + 1u;
}

uint32_t GetNextBallId(
	const MagnetStageBallPlacement* placements,
	std::size_t count) noexcept
{
	uint32_t maximumId = 0;
	for (std::size_t index = 0; index < count; ++index) {
		maximumId = (std::max)(maximumId, placements[index].id);
	}
	return maximumId == UINT32_MAX ? 0u : maximumId + 1u;
}

std::size_t CountTransferGateEndpoints(
	const MagnetStageData& stageData,
	uint32_t pairId,
	uint32_t selectedId,
	uint32_t& partnerId) noexcept
{
	partnerId = 0;
	std::size_t endpointCount = 0;
	const std::size_t obstacleCount =
		(std::min)(stageData.obstacleCount, stageData.obstacles.size());
	for (std::size_t index = 0; index < obstacleCount; ++index) {
		const MagnetStageBoxPlacement& obstacle = stageData.obstacles[index];
		if (obstacle.obstacleKind != MagnetObstacleKind::TransferGate ||
			obstacle.transferPairId != pairId) {
			continue;
		}
		++endpointCount;
		if (obstacle.id != selectedId) {
			partnerId = obstacle.id;
		}
	}
	return endpointCount;
}

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
				char label[96]{};
				if (obstacle.obstacleKind == MagnetObstacleKind::TransferGate) {
					std::snprintf(
						label,
						sizeof(label),
						"転送ゲート ペア%u / ID%u###StageObstacle%u",
						obstacle.transferPairId,
						obstacle.id,
						obstacle.id);
				} else {
					std::snprintf(
						label,
						sizeof(label),
						"%s %u###StageObstacle%u",
						GetObstacleKindName(obstacle.obstacleKind),
						obstacle.id,
						obstacle.id);
				}
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

			ImDrawList* drawList = ImGui::GetWindowDrawList();
			char scoreText[64]{};
			std::snprintf(scoreText, sizeof(scoreText), "SCORE  %zu", viewData.score);
			const ImVec2 scoreMinimum = {
				imagePosition.x + kMinimapMargin,
				imagePosition.y + kMinimapMargin,
			};
			const ImVec2 scoreTextSize = ImGui::CalcTextSize(scoreText);
			const ImVec2 scoreMaximum = {
				scoreMinimum.x + scoreTextSize.x + 22.0f,
				scoreMinimum.y + scoreTextSize.y + 14.0f,
			};
			drawList->AddRectFilled(scoreMinimum, scoreMaximum, IM_COL32(10, 16, 24, 205), 7.0f);
			drawList->AddRect(scoreMinimum, scoreMaximum, IM_COL32(255, 215, 70, 235), 7.0f, 0, 2.0f);
			drawList->AddText({ scoreMinimum.x + 11.0f, scoreMinimum.y + 7.0f },
				IM_COL32(255, 230, 100, 255), scoreText);
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

		ImGui::SeparatorText("作業モード");
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

		if (viewData.editorMode == MagnetEditorMode::Play) {
			ImGui::SeparatorText("プレイ操作");
			const float spacing = ImGui::GetStyle().ItemSpacing.x;
			const float availableWidth =
				(std::max)(ImGui::GetContentRegionAvail().x, 1.0f);
			const bool stackButtons =
				availableWidth < kMinimumButtonWidth * 2.0f + spacing;
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
			ImGui::Text(
				"左 %zu / %zu",
				viewData.leftChainCount,
				MagnetChainSystem::kLinksPerSide);
			ImGui::Text(
				"右 %zu / %zu",
				viewData.rightChainCount,
				MagnetChainSystem::kLinksPerSide);
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

			ImGui::SeparatorText("ステージ配置");
			ImGui::TextWrapped(
				"配置を変更するときは、上の「ステージ配置」へ切り替えてください。");
		} else {
			ImGui::TextColored(
				ImVec4{ 0.35f, 0.90f, 0.50f, 1.0f },
				"配置編集中 - シミュレーションは一時停止しています");
			DrawStageEditor(viewData, request);
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

	ImGui::SeparatorText("選択中のオブジェクト");
	if (stageData && selectedObjectType_ == MagnetStageObjectType::Player) {
		ImGui::TextColored(
			ImVec4(1.0f, 0.35f, 0.35f, 1.0f),
			"プレイヤー専用設定");
		ImGui::TextUnformatted("種類: プレイヤー");
		ImGui::TextDisabled("開始位置を編集します。高さは地面に固定されます。");
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
			"小さい球専用設定");
		ImGui::Text(
			"種類: 小さい球  |  固有ID: %u",
			selectedBall->id);
		ImGui::TextDisabled("吸着前の位置を編集します。高さは地面に固定されます。");
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
		ImGui::Spacing();
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.58f, 0.14f, 0.12f, 1.0f });
		if (ImGui::Button("選択した球を削除", { -1.0f, 0.0f })) {
			request.stageAction = MagnetStageEditorAction::RemoveBall;
			request.selectedBallId = selectedBall->id;
		}
		ImGui::PopStyleColor();
	} else if (selectedBox) {
		const bool isGoal = selectedObjectType_ == MagnetStageObjectType::Goal;
		ImGui::TextColored(
			isGoal
				? ImVec4(0.25f, 1.0f, 0.45f, 1.0f)
				: ImVec4(1.0f, 0.55f, 0.20f, 1.0f),
			isGoal ? "ゴール専用設定" : "障害物専用設定");
		if (isGoal) {
			ImGui::Text("種類: ゴール  |  固有ID: %u", selectedBox->id);
		} else {
			ImGui::Text(
				"種類: %s  |  固有ID: %u",
				GetObstacleKindName(selectedBox->obstacleKind),
				selectedBox->id);
		}
		ImGui::TextDisabled(
			isGoal
				? "位置と得点判定エリアの大きさを編集します。"
				: "この障害物だけの位置・サイズ・種類を編集します。");
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
		if (isGoal) {
			int score = static_cast<int>(selectedBox->score);
			ImGui::SetNextItemWidth(-1.0f);
			if (ImGui::InputInt("得点##GoalScore", &score)) {
				score = std::clamp(score, 1, 999);
				request.stageAction = MagnetStageEditorAction::UpdateGoalScore;
				request.selectedObjectId = selectedBox->id;
				request.editedGoalScore = static_cast<uint32_t>(score);
			}
		} else {
			const char* obstacleKinds[kObstacleKindCount] = {
				"通常の壁", "チェーンソー", "ピンボールバンパー",
				"溶鉱炉", "磁石アンカー", "開閉シャッター", "転送ゲート",
				"反発磁場",
			};
			int obstacleKind = static_cast<int>(selectedBox->obstacleKind);
			ImGui::SetNextItemWidth(-1.0f);
			if (ImGui::Combo(
				"障害物の種類##SelectedObstacleKind",
				&obstacleKind,
				obstacleKinds,
				kObstacleKindCount)) {
				request.stageAction = MagnetStageEditorAction::UpdateObstacleKind;
				request.selectedObjectId = selectedBox->id;
				request.editedObstacleKind =
					static_cast<MagnetObstacleKind>(obstacleKind);
			}
			ImGui::TextWrapped("%s", GetObstacleKindDescription(selectedBox->obstacleKind));
			if (selectedBox->obstacleKind == MagnetObstacleKind::TransferGate) {
				uint32_t partnerId = 0;
				const std::size_t endpointCount = stageData
					? CountTransferGateEndpoints(
						*stageData,
						selectedBox->transferPairId,
						selectedBox->id,
						partnerId)
					: 0;
				ImGui::SeparatorText("転送ゲートの接続");
				ImGui::Text("このゲートの固有ID: %u", selectedBox->id);
				ImGui::TextDisabled("固有IDはゲートごとに違っていて正常です。");
				int pairId = static_cast<int>((std::min)(
					selectedBox->transferPairId,
					static_cast<uint32_t>(INT_MAX)));
				ImGui::SetNextItemWidth(-1.0f);
				if (ImGui::InputInt("接続ペア番号##TransferPairId", &pairId)) {
					pairId = (std::max)(pairId, 1);
					request.stageAction =
						MagnetStageEditorAction::UpdateTransferPairId;
					request.selectedObjectId = selectedBox->id;
					request.editedTransferPairId = static_cast<uint32_t>(pairId);
				}
				if (endpointCount == 2) {
					ImGui::TextColored(
						ImVec4{ 0.30f, 0.95f, 0.48f, 1.0f },
						"接続済み: ペア%u / 相手の固有ID %u",
						selectedBox->transferPairId,
						partnerId);
				} else if (endpointCount < 2) {
					ImGui::TextColored(
						ImVec4{ 1.0f, 0.68f, 0.20f, 1.0f },
						"未接続: 同じペア番号のゲートがあと1個必要です。");
				} else {
					ImGui::TextColored(
						ImVec4{ 1.0f, 0.25f, 0.20f, 1.0f },
						"設定エラー: ペア%uが%zu個あります。2個にしてください。",
						selectedBox->transferPairId,
						endpointCount);
				}
				ImGui::TextDisabled(
					"入口と出口は同じペア番号にします。X/Zの短い辺が通過方向です。");
			}
		}
		ImGui::Spacing();
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.58f, 0.14f, 0.12f, 1.0f });
		if (ImGui::Button(
			isGoal ? "選択したゴールを削除" : "選択した障害物を削除",
			{ -1.0f, 0.0f })) {
			request.stageAction = MagnetStageEditorAction::RemoveBoxObject;
			request.selectedObjectType = selectedObjectType_;
			request.selectedObjectId = selectedBox->id;
		}
		ImGui::PopStyleColor();
	} else {
		ImGui::TextColored(
			ImVec4{ 0.55f, 0.72f, 0.88f, 1.0f },
			"オブジェクトが選択されていません");
		ImGui::TextWrapped(
			"左の「ステージ階層」からプレイヤー、小さい球、ゴール、または障害物を選択してください。");
	}

	ImGui::SeparatorText("新しいオブジェクトを追加");
	ImGui::TextDisabled("種類を選び、追加後に表示される専用設定で配置します。");
	if (ImGui::BeginTabBar("StageObjectAddTabs")) {
		if (ImGui::BeginTabItem("小さい球")) {
			ImGui::TextWrapped(
				"吸着できる小さい球をプレイヤーの前方へ1個追加します。");
			const bool ballCapacityReached = stageData &&
				stageData->ballCount >= stageData->balls.size();
			if (!stageData || ballCapacityReached) {
				ImGui::BeginDisabled();
			}
			if (ImGui::Button("小さい球を追加して選択", { -1.0f, 34.0f })) {
				Vector3 position{ 0.0f, 0.5f, 4.0f };
				if (stageData) {
					position.x = stageData->playerPosition.x;
					position.z = std::clamp(
						stageData->playerPosition.z + 4.0f,
						stageData->generation.minimumZ,
						stageData->generation.maximumZ);
					selectedObjectType_ = MagnetStageObjectType::MagnetBall;
					selectedObjectId_ = GetNextBallId(
						stageData->balls.data(), stageData->ballCount);
				}
				request.stageAction = MagnetStageEditorAction::AddBall;
				request.editedObjectPosition = position;
			}
			if (!stageData || ballCapacityReached) {
				ImGui::EndDisabled();
			}
			if (ballCapacityReached) {
				ImGui::TextColored(
					ImVec4{ 1.0f, 0.40f, 0.30f, 1.0f },
					"小さい球は上限数に達しています。");
			}
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("ゴール")) {
			ImGui::TextWrapped(
				"小さい球が入ると得点になる判定エリアを追加します。");
			const bool goalCapacityReached = stageData &&
				stageData->goalCount >= stageData->goals.size();
			if (!stageData || goalCapacityReached) {
				ImGui::BeginDisabled();
			}
			if (ImGui::Button("ゴールを追加して選択", { -1.0f, 34.0f })) {
				Vector3 position{ 0.0f, 1.0f, 8.0f };
				if (stageData) {
					position.x = stageData->playerPosition.x;
					position.z = std::clamp(
						stageData->playerPosition.z + 8.0f,
						stageData->generation.minimumZ,
						stageData->generation.maximumZ);
					selectedObjectType_ = MagnetStageObjectType::Goal;
					selectedObjectId_ = GetNextBoxId(
						stageData->goals.data(), stageData->goalCount);
				}
				request.stageAction = MagnetStageEditorAction::AddGoal;
				request.editedObjectPosition = position;
				request.editedObjectSize = { 2.5f, 2.0f, 1.0f };
			}
			if (!stageData || goalCapacityReached) {
				ImGui::EndDisabled();
			}
			if (goalCapacityReached) {
				ImGui::TextColored(
					ImVec4{ 1.0f, 0.40f, 0.30f, 1.0f },
					"ゴールは上限数に達しています。");
			}
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem("障害物")) {
			const char* obstacleKinds[kObstacleKindCount] = {
				"通常の壁", "チェーンソー", "ピンボールバンパー",
				"溶鉱炉", "磁石アンカー", "開閉シャッター", "転送ゲート",
				"反発磁場",
			};
			int paletteKind = static_cast<int>(obstaclePaletteKind_);
			ImGui::SetNextItemWidth(-1.0f);
			if (ImGui::Combo(
				"追加する障害物##ObstaclePaletteKind",
				&paletteKind,
				obstacleKinds,
				kObstacleKindCount)) {
				obstaclePaletteKind_ = static_cast<MagnetObstacleKind>(paletteKind);
			}
			ImGui::TextWrapped("%s", GetObstacleKindDescription(obstaclePaletteKind_));
			const bool obstacleCapacityReached = stageData &&
				stageData->obstacleCount >= stageData->obstacles.size();
			if (!stageData || obstacleCapacityReached) {
				ImGui::BeginDisabled();
			}
			if (ImGui::Button("この障害物を追加して選択", { -1.0f, 34.0f })) {
				const Vector3 size = GetObstacleDefaultSize(obstaclePaletteKind_);
				Vector3 position{ 0.0f, size.y * 0.5f, 4.0f };
				if (stageData) {
					position.x = stageData->playerPosition.x;
					position.z = std::clamp(
						stageData->playerPosition.z + 4.0f,
						stageData->generation.minimumZ,
						stageData->generation.maximumZ);
					selectedObjectType_ = MagnetStageObjectType::Obstacle;
					selectedObjectId_ = GetNextBoxId(
						stageData->obstacles.data(), stageData->obstacleCount);
				}
				request.stageAction = MagnetStageEditorAction::AddObstacle;
				request.editedObjectPosition = position;
				request.editedObjectSize = size;
				request.editedObstacleKind = obstaclePaletteKind_;
			}
			if (!stageData || obstacleCapacityReached) {
				ImGui::EndDisabled();
			}
			if (obstacleCapacityReached) {
				ImGui::TextColored(
					ImVec4{ 1.0f, 0.40f, 0.30f, 1.0f },
					"障害物は上限数に達しています。");
			}
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}

	ImGui::Separator();
	if (ImGui::TreeNodeEx("ステージ全体の設定###StageWideSettings")) {
		if (stageData) {
			float arenaRadius = stageData->arenaRadius;
			ImGui::SetNextItemWidth(-1.0f);
			if (ImGui::SliderFloat(
				"ステージ半径",
				&arenaRadius,
				4.0f,
				40.0f,
				"%.1f")) {
				request.stageAction = MagnetStageEditorAction::SetArenaRadius;
				request.arenaRadius = arenaRadius;
			}
			ImGui::TextDisabled("円形の床と外周壁の大きさを変更します。");
		}

		if (ImGui::TreeNodeEx("小さい球のランダム配置")) {
			int ballCount = static_cast<int>(generationSettings_.ballCount);
			if (ImGui::SliderInt(
				"球の数",
				&ballCount,
				1,
				static_cast<int>(MagnetStageData::kMaximumBallCount))) {
				generationSettings_.ballCount = static_cast<std::size_t>(ballCount);
			}
			ImGui::InputScalar(
				"乱数シード",
				ImGuiDataType_U32,
				&generationSettings_.seed);
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
			if (ImGui::Button(
				"現在の小さい球をランダム再配置",
				{ -1.0f, 32.0f })) {
				request.stageAction = MagnetStageEditorAction::GenerateBalanced;
				request.generationSettings = generationSettings_;
			}
			ImGui::TreePop();
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
			ImGui::Text("%zu", viewData.score);
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
