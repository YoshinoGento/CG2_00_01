#include "editor/ObjectInspectorWindow.h"

#ifdef USE_IMGUI
#include "base/ImGuiManager.h"
#include <algorithm>
#endif

namespace {
constexpr float kRadiansToDegrees = 180.0f / 3.141592f;
constexpr float kDegreesToRadians = 3.141592f / 180.0f;
}

editor::ObjectInspectorEditorCommand ObjectInspectorWindow::Draw(
	const editor::ObjectInspectorEditorViewData& viewData) {
	editor::ObjectInspectorEditorCommand command;
	command.directionalLight = viewData.directionalLight;
	command.spotLight = viewData.spotLight;
	command.sphere = viewData.sphere;
	command.animation = viewData.animation;
	command.cylinder = viewData.cylinder;
#ifdef USE_IMGUI
	if (!open_) {
		return command;
	}
	if (!ImGui::Begin("Object Inspector", &open_)) {
		ImGui::End();
		return command;
	}

	if (ImGui::CollapsingHeader("Directional Light")) {
		command.directionalLightChanged |= ImGui::DragFloat3(
			"Direction", &command.directionalLight.direction.x, 0.01f, -1.0f, 1.0f);
		command.directionalLightChanged |= ImGui::ColorEdit3(
			"Color", &command.directionalLight.color.x);
		command.directionalLightChanged |= ImGui::SliderFloat(
			"Intensity", &command.directionalLight.intensity, 0.0f, 10.0f);
	}

	if (ImGui::CollapsingHeader("Spot Light", ImGuiTreeNodeFlags_DefaultOpen)) {
		command.spotLightChanged |= ImGui::ColorEdit3("Color##Spot", &command.spotLight.color.x);
		command.spotLightChanged |= ImGui::DragFloat3("Position##Spot", &command.spotLight.position.x, 0.1f);
		command.spotLightChanged |= ImGui::SliderFloat(
			"Intensity##Spot", &command.spotLight.intensity, 0.0f, 20.0f);
		command.spotLightChanged |= ImGui::DragFloat3(
			"Direction##Spot", &command.spotLight.direction.x, 0.01f, -1.0f, 1.0f);
		command.spotLightChanged |= ImGui::SliderFloat(
			"Distance##Spot", &command.spotLight.distance, 1.0f, 100.0f);
		command.spotLightChanged |= ImGui::SliderFloat(
			"Decay##Spot", &command.spotLight.decay, 0.1f, 10.0f);
		float angleDegrees = command.spotLight.angleRadians * kRadiansToDegrees;
		if (ImGui::SliderFloat("Beam Angle", &angleDegrees, 0.0f, 90.0f)) {
			command.spotLight.angleRadians = angleDegrees * kDegreesToRadians;
			command.spotLightChanged = true;
		}
		float falloffDegrees = command.spotLight.falloffRadians * kRadiansToDegrees;
		if (ImGui::SliderFloat("Falloff Start", &falloffDegrees, 0.0f, 90.0f)) {
			command.spotLight.falloffRadians = falloffDegrees * kDegreesToRadians;
			command.spotLightChanged = true;
		}
	}

	if (ImGui::CollapsingHeader("Sphere (3D)")) {
		command.sphereGeometryChanged |= ImGui::SliderFloat(
			"Radius", &command.sphere.radius, 0.1f, 10.0f);
		command.sphereTransformChanged |= ImGui::DragFloat3(
			"Position##Sphere", &command.sphere.position.x, 0.1f);
		command.sphereTransformChanged |= ImGui::DragFloat3(
			"Rotation##Sphere", &command.sphere.rotation.x, 0.01f);
	}

	if (ImGui::CollapsingHeader("Animation Control", ImGuiTreeNodeFlags_DefaultOpen)) {
		constexpr const char* kAnimationModels[] = {
			"AnimatedCube",
			"simpleSkin",
			"human/walk",
			"human/sneakWalk",
		};
		command.animationModelChanged |= ImGui::Combo(
			"Model", &command.animation.modelIndex, kAnimationModels, 4);
		command.animationVisibilityChanged |= ImGui::Checkbox(
			"Show Model", &command.animation.showModel);
		if (command.animation.hasObject) {
			command.animationPlaybackChanged |= ImGui::Checkbox(
				"Play Animation", &command.animation.playing);
			command.animationPlaybackChanged |= ImGui::SliderFloat(
				"Speed", &command.animation.speed, -5.0f, 5.0f);
			if (command.animation.duration > 0.0f) {
				command.animationPlaybackChanged |= ImGui::SliderFloat(
					"Time", &command.animation.time, 0.0f, command.animation.duration);
			}
		}
	}

	if (ImGui::CollapsingHeader("Cylinder (Effect)", ImGuiTreeNodeFlags_DefaultOpen)) {
		command.cylinderChanged |= ImGui::SliderFloat(
			"Top Radius", &command.cylinder.topRadius, 0.0f, 5.0f);
		command.cylinderChanged |= ImGui::SliderFloat(
			"Bottom Radius", &command.cylinder.bottomRadius, 0.0f, 5.0f);
		command.cylinderChanged |= ImGui::SliderFloat(
			"Height##Cylinder", &command.cylinder.height, 0.1f, 10.0f);
		command.cylinderChanged |= ImGui::SliderInt(
			"Segments", &command.cylinder.segments, 3, 64);
		command.cylinderChanged |= ImGui::SliderInt(
			"Vertical Divisions", &command.cylinder.verticalDivisions, 1, 16);
	}

	if (!viewData.joints.empty() &&
		ImGui::CollapsingHeader("Skeleton Bones", ImGuiTreeNodeFlags_DefaultOpen)) {
		selectedJointIndex_ = std::clamp(
			selectedJointIndex_, 0, static_cast<int>(viewData.joints.size()) - 1);
		const char* previewName = viewData.joints[static_cast<std::size_t>(selectedJointIndex_)].name.c_str();
		if (ImGui::BeginCombo("Target Joint", previewName)) {
			for (std::size_t index = 0; index < viewData.joints.size(); ++index) {
				const bool selected = static_cast<int>(index) == selectedJointIndex_;
				if (ImGui::Selectable(viewData.joints[index].name.c_str(), selected)) {
					selectedJointIndex_ = static_cast<int>(index);
				}
				if (selected) {
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		command.joint = viewData.joints[static_cast<std::size_t>(selectedJointIndex_)];
		ImGui::Text("Active: %s (Index: %d)", command.joint.name.c_str(), command.joint.jointIndex);
		command.jointChanged |= ImGui::DragFloat3(
			"Bone Translate", &command.joint.translate.x, 0.05f);
		command.jointChanged |= ImGui::DragFloat3(
			"Bone Scale", &command.joint.scale.x, 0.05f, 0.01f, 10.0f);
		ImGui::TextDisabled("Bone rotation editing is not implemented.");
	}
	ImGui::End();
#else
	(void)viewData;
#endif
	return command;
}

void ObjectInspectorWindow::SetOpen(bool open) noexcept {
	open_ = open;
}

bool ObjectInspectorWindow::IsOpen() const noexcept {
	return open_;
}
