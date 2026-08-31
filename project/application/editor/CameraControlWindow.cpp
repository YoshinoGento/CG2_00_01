#include "editor/CameraControlWindow.h"

#ifdef USE_IMGUI
#include "base/ImGuiManager.h"
#endif

editor::CameraEditorCommand CameraControlWindow::Draw(const editor::CameraEditorViewData& viewData) {
	editor::CameraEditorCommand command;
	command.value = viewData;
#ifdef USE_IMGUI
	if (!open_) {
		return command;
	}
	if (!ImGui::Begin("Camera Control", &open_)) {
		ImGui::End();
		return command;
	}

	command.apply |= ImGui::DragFloat3("Camera Position", &command.value.position.x, 0.1f);
	command.apply |= ImGui::DragFloat3("Camera Rotation", &command.value.rotation.x, 0.01f);
	command.apply |= ImGui::DragFloat("Move Speed", &command.value.moveSpeed, 0.1f, 0.0f, 100.0f);
	command.apply |= ImGui::DragFloat("Rotate Speed", &command.value.rotateSpeed, 0.01f, 0.0f, 10.0f);
	if (ImGui::Button("Reset Camera")) {
		command.reset = true;
	}
	ImGui::End();
#else
	(void)viewData;
#endif
	return command;
}

void CameraControlWindow::SetOpen(bool open) noexcept {
	open_ = open;
}

bool CameraControlWindow::IsOpen() const noexcept {
	return open_;
}
