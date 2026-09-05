#include "editor/GameViewportWindow.h"

#include "base/ImGuiManager.h"
#include "base/SrvManager.h"

#include <algorithm>

void GameViewportWindow::Draw(
	SrvManager& srvManager,
	uint32_t finalDisplaySrvIndex,
	const Vector2& virtualResolution,
	EditorLanguage language,
	bool expanded) {
	frameState_ = {};

#ifdef USE_IMGUI
	const ImGuiViewport* mainViewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(mainViewport->WorkPos, expanded ? ImGuiCond_Always : ImGuiCond_FirstUseEver);
	ImGui::SetNextWindowSize(
		expanded ? mainViewport->WorkSize : ImVec2{ (std::max)(mainViewport->WorkSize.x - 300.0f, 320.0f), mainViewport->WorkSize.y },
		expanded ? ImGuiCond_Always : ImGuiCond_FirstUseEver);
	const ImGuiWindowFlags flags = expanded
		? ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDocking |
			ImGuiWindowFlags_NoSavedSettings
		: ImGuiWindowFlags_None;
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	if (ImGui::Begin(expanded ? "FarmGameView" : editor::Localize(language, "Game Viewport###GameViewport"), nullptr, flags)) {
		frameState_.focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
		const ImVec2 contentSize = ImGui::GetContentRegionAvail();
		if (contentSize.x > 1.0f && contentSize.y > 1.0f &&
			virtualResolution.x > 1.0f && virtualResolution.y > 1.0f) {
			const float targetAspect = virtualResolution.x / virtualResolution.y;
			ImVec2 displaySize = contentSize;
			if (displaySize.x / displaySize.y > targetAspect) {
				displaySize.x = displaySize.y * targetAspect;
			} else {
				displaySize.y = displaySize.x / targetAspect;
			}

			if (displaySize.x > 1.0f && displaySize.y > 1.0f) {
				const ImVec2 offset = {
					(contentSize.x - displaySize.x) * 0.5f,
					(contentSize.y - displaySize.y) * 0.5f,
				};
				const ImVec2 cursorPosition = ImGui::GetCursorPos();
				ImGui::SetCursorPos({ cursorPosition.x + offset.x, cursorPosition.y + offset.y });

				ImGui::Image(
					static_cast<ImTextureID>(srvManager.GetGPUDescriptorHandle(finalDisplaySrvIndex).ptr),
					displaySize);

				const ImVec2 imageTopLeft = ImGui::GetItemRectMin();
				const ImVec2 imageBottomRight = ImGui::GetItemRectMax();
				const ImVec2 mousePosition = ImGui::GetIO().MousePos;
				const Vector2 actualDisplaySize = {
					imageBottomRight.x - imageTopLeft.x,
					imageBottomRight.y - imageTopLeft.y,
				};

				frameState_.imageTopLeft = { imageTopLeft.x, imageTopLeft.y };
				frameState_.imageSize = actualDisplaySize;
				frameState_.mousePosition = { mousePosition.x, mousePosition.y };
				frameState_.virtualMousePosition = {
					(mousePosition.x - imageTopLeft.x) / actualDisplaySize.x * virtualResolution.x,
					(mousePosition.y - imageTopLeft.y) / actualDisplaySize.y * virtualResolution.y,
				};
				frameState_.hovered = ImGui::IsItemHovered();
				frameState_.leftClicked = frameState_.hovered &&
					ImGui::IsMouseClicked(ImGuiMouseButton_Left);
				frameState_.quickApplyRequested = frameState_.leftClicked &&
					ImGui::GetIO().KeyShift;
				frameState_.imageVisible = true;
			}
		}
	}
	ImGui::End();
	ImGui::PopStyleVar();
#else
	(void)srvManager;
	(void)finalDisplaySrvIndex;
	(void)virtualResolution;
	(void)language;
	(void)expanded;
#endif
}
