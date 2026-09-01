#include "application/magnet/ui/MagnetPrototypeWindow.h"

#include "base/SrvManager.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui_internal.h"
#endif

#include <algorithm>
#include <cmath>

namespace magnet {

namespace {

constexpr char kDockspaceName[] = "MagnetPrototypeDockSpace";
constexpr char kHierarchyWindowName[] = "Magnet Scene###MagnetHierarchy";
constexpr char kViewportWindowName[] = "Game View###MagnetViewport";
constexpr char kInspectorWindowName[] = "Physics Inspector###MagnetInspector";
constexpr char kMonitorWindowName[] = "Simulation Monitor###MagnetMonitor";
constexpr float kLeftPanelRatio = 0.18f;
constexpr float kRightPanelRatio = 0.25f;
constexpr float kBottomPanelRatio = 0.22f;
constexpr float kMinimumButtonWidth = 80.0f;
constexpr float kConstraintWarningError = 0.08f;

} // namespace

MagnetPrototypeUiRequest MagnetPrototypeWindow::Draw(
	const MagnetPrototypeViewData& viewData,
	SrvManager* srvManager,
	uint32_t finalDisplaySrvIndex,
	float virtualWidth,
	float virtualHeight)
{
	MagnetPrototypeUiRequest request{};
#ifdef USE_IMGUI
	DrawMainMenuBar(viewData);

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

	request.emitterSettings = emitterSettings_;
	request.showGrid = showGrid_;
	request.showVelocity = showVelocity_;
	request.cameraFollow = cameraFollow_;
	return request;
}

void MagnetPrototypeWindow::DrawMainMenuBar(const MagnetPrototypeViewData& viewData)
{
#ifdef USE_IMGUI
	if (!ImGui::BeginMainMenuBar()) {
		return;
	}

	ImGui::TextUnformatted("MAGNET CHAIN PHYSICS");
	ImGui::Separator();
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
		ImGui::TextDisabled("SIMULATION OBJECTS");
		ImGui::Separator();
		ImGui::BulletText("Player (Kinematic)");
		ImGui::TextColored(
			viewData.chainsAttached
				? ImVec4{ 0.25f, 0.95f, 0.45f, 1.0f }
				: ImVec4{ 1.0f, 0.65f, 0.18f, 1.0f },
			viewData.chainsAttached ? "Magnet: ATTACHED" : "Magnet: RELEASED");

		if (ImGui::TreeNodeEx("Left Chain", ImGuiTreeNodeFlags_DefaultOpen)) {
			for (std::size_t index = 0; index < MagnetChainSystem::kLinksPerSide; ++index) {
				ImGui::BulletText("Link %zu", index + 1);
			}
			ImGui::TreePop();
		}
		if (ImGui::TreeNodeEx("Right Chain", ImGuiTreeNodeFlags_DefaultOpen)) {
			for (std::size_t index = 0; index < MagnetChainSystem::kLinksPerSide; ++index) {
				ImGui::BulletText("Link %zu", index + 1);
			}
			ImGui::TreePop();
		}
		if (ImGui::TreeNodeEx("Test Ball Pool", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Text("Active: %zu / %zu",
				viewData.activeTestBallCount,
				MagnetChainSystem::kTestBallCapacity);
			ImGui::TextDisabled("Fixed-capacity round-robin Pool");
			ImGui::TreePop();
		}
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
		request.reset = ImGui::Button("RESET", { buttonWidth, 36.0f });

		ImGui::SeparatorText("Magnetic Coupling");
		ImGui::TextWrapped(
			"Release the left/right chains with their current velocity. "
			"The Player receives no release impulse.");
		if (!viewData.chainsAttached) {
			ImGui::BeginDisabled();
		}
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.88f, 0.38f, 0.08f, 1.0f });
		request.releaseChains = ImGui::Button(
			"MAGNET OFF - RELEASE CHAINS",
			{ (std::max)(ImGui::GetContentRegionAvail().x, 1.0f), 36.0f });
		ImGui::PopStyleColor();
		if (!viewData.chainsAttached) {
			ImGui::EndDisabled();
			ImGui::TextDisabled("RESET reattaches the chains.");
		}

		ImGui::SeparatorText("Test Ball Emitter");
		ImGui::TextDisabled("Separate future attraction test; not chain release.");
		request.emitOne = ImGui::Button(
			"EMIT ONE",
			{ (std::max)(ImGui::GetContentRegionAvail().x, 1.0f), 32.0f });
		ImGui::Checkbox("Auto Emit", &emitterSettings_.autoEmit);
		ImGui::TextUnformatted("Interval");
		ImGui::SetNextItemWidth(-1.0f);
		ImGui::SliderFloat(
			"###EmitterInterval",
			&emitterSettings_.intervalSeconds,
			0.05f,
			2.0f,
			"%.2f s");
		ImGui::TextUnformatted("Launch Speed");
		ImGui::SetNextItemWidth(-1.0f);
		ImGui::SliderFloat(
			"###EmitterLaunchSpeed",
			&emitterSettings_.launchSpeed,
			1.0f,
			20.0f,
			"%.1f");

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

void MagnetPrototypeWindow::DrawMonitor(const MagnetPrototypeViewData& viewData)
{
#ifdef USE_IMGUI
	if (ImGui::Begin(kMonitorWindowName, nullptr, ImGuiWindowFlags_NoCollapse)) {
		if (ImGui::BeginTable(
			"MagnetMetrics",
			4,
			ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchSame)) {
			ImGui::TableNextColumn();
			ImGui::TextDisabled("BODIES");
			ImGui::Text("%zu", viewData.bodyCount);
			ImGui::TableNextColumn();
			ImGui::TextDisabled("CONSTRAINTS");
			ImGui::Text(
				"%zu / %zu active",
				viewData.activeConstraintCount,
				viewData.constraintCount);
			ImGui::TableNextColumn();
			ImGui::TextDisabled("PLAYER SPEED");
			ImGui::Text("%.2f", viewData.playerSpeed);
			ImGui::TableNextColumn();
			ImGui::TextDisabled("ACTIVE TEST BALLS");
			ImGui::Text("%zu / %zu",
				viewData.activeTestBallCount,
				MagnetChainSystem::kTestBallCapacity);
			ImGui::EndTable();
		}

		ImGui::Spacing();
		const bool finiteConstraintError =
			std::isfinite(viewData.maximumConstraintError) &&
			viewData.maximumConstraintError >= 0.0f;
		if (finiteConstraintError) {
			ImGui::Text(
				"Maximum constraint error: %.4f",
				viewData.maximumConstraintError);
		} else {
			ImGui::TextColored(
				ImVec4{ 1.0f, 0.25f, 0.20f, 1.0f },
				"Maximum constraint error: NON-FINITE");
		}
		const float normalizedError = finiteConstraintError
			? std::clamp(
				viewData.maximumConstraintError / kConstraintWarningError,
				0.0f,
				1.0f)
			: 1.0f;
		ImGui::ProgressBar(normalizedError, { -1.0f, 0.0f }, "constraint stability");
		ImGui::TextDisabled(
			"WASD Move  |  Space Stop  |  Q Magnet OFF  |  E Test Emit  |  R Reset");
	}
	ImGui::End();
#else
	(void)viewData;
#endif
}

} // namespace magnet
