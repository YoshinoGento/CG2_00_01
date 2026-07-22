#include "editor/VisibilityWindow.h"

#ifdef USE_IMGUI
#include "base/ImGuiManager.h"
#endif

editor::VisibilityEditorCommand VisibilityWindow::Draw(
	const editor::VisibilityEditorViewData& viewData) {
	editor::VisibilityEditorCommand command;
	command.value = viewData;
#ifdef USE_IMGUI
	if (!open_) {
		return command;
	}
	if (!ImGui::Begin("Visibility & Cull", &open_)) {
		ImGui::End();
		return command;
	}

	constexpr const char* kTargets[] = { "None", "Sprite", "Object3D", "Particle", "Sphere" };
	command.apply |= ImGui::Combo("Edit Focus", &command.value.selectedTarget, kTargets, 5);
	ImGui::Separator();
	command.apply |= ImGui::Checkbox("Terrain", &command.value.showTerrain);
	command.apply |= ImGui::Checkbox("Sphere", &command.value.showSphere);
	command.apply |= ImGui::Checkbox("Plane", &command.value.showPlane);
	command.apply |= ImGui::Checkbox("Sprite (2D)", &command.value.showSprite);
	command.apply |= ImGui::Checkbox("Particles", &command.value.showParticles);
	command.apply |= ImGui::Checkbox("Animated Model", &command.value.showAnimatedModel);
	command.apply |= ImGui::Checkbox("Skeleton Debug", &command.value.showSkeleton);
	ImGui::Separator();
	constexpr const char* kCullModes[] = { "None (Two-sided)", "Front", "Back" };
	command.apply |= ImGui::Combo("Cull Mode", &command.value.cullMode, kCullModes, 3);
	ImGui::End();
#else
	(void)viewData;
#endif
	return command;
}

void VisibilityWindow::SetOpen(bool open) noexcept {
	open_ = open;
}

bool VisibilityWindow::IsOpen() const noexcept {
	return open_;
}
