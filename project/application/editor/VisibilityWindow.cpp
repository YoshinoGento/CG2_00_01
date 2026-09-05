#include "editor/VisibilityWindow.h"

#ifdef USE_IMGUI
#include "base/ImGuiManager.h"
#endif

namespace {
void ShowAll(editor::VisibilityEditorViewData& value) noexcept {
	value.showFarmMeshes = true;
	value.showTerrain = true;
	value.showSphere = true;
	value.showPlane = true;
	value.showSprite = true;
	value.showParticles = true;
	value.showAnimatedModel = true;
	value.showSkeleton = true;
	value.showLevelObjects = true;
	value.showLevelGizmos = true;
	value.showCollisionGizmos = true;
}

void ApplyFarmEditingPreset(editor::VisibilityEditorViewData& value) noexcept {
	value.showFarmMeshes = true;
	value.showTerrain = false;
	value.showSphere = false;
	value.showPlane = false;
	value.showSprite = false;
	value.showParticles = true;
	value.showAnimatedModel = false;
	value.showSkeleton = false;
	value.showLevelObjects = true;
	value.showLevelGizmos = false;
	value.showCollisionGizmos = false;
}

void ApplyCleanCapturePreset(editor::VisibilityEditorViewData& value) noexcept {
	ApplyFarmEditingPreset(value);
	value.showLevelObjects = false;
}

void HideDebugVisualizations(editor::VisibilityEditorViewData& value) noexcept {
	value.showSkeleton = false;
	value.showLevelGizmos = false;
	value.showCollisionGizmos = false;
}

void ResetVisibility(editor::VisibilityEditorViewData& value) noexcept {
	value.showFarmMeshes = true;
	value.showTerrain = true;
	value.showSphere = true;
	value.showPlane = true;
	value.showSprite = true;
	value.showParticles = true;
	value.showAnimatedModel = false;
	value.showSkeleton = false;
	value.showLevelObjects = true;
	value.showLevelGizmos = false;
	value.showCollisionGizmos = false;
	value.cullMode = 2;
}

int CountVisibleItems(const editor::VisibilityEditorViewData& value) noexcept {
	return static_cast<int>(value.showFarmMeshes) + static_cast<int>(value.showTerrain) +
		static_cast<int>(value.showSphere) +
		static_cast<int>(value.showPlane) +
		static_cast<int>(value.showSprite) +
		static_cast<int>(value.showParticles) +
		static_cast<int>(value.showAnimatedModel) +
		static_cast<int>(value.showSkeleton) +
		static_cast<int>(value.showLevelObjects) +
		static_cast<int>(value.showLevelGizmos) +
		static_cast<int>(value.showCollisionGizmos);
}
} // namespace

editor::VisibilityEditorCommand VisibilityWindow::Draw(
	const editor::VisibilityEditorViewData& viewData,
	EditorLanguage language) {
	editor::VisibilityEditorCommand command;
	command.value = viewData;
#ifdef USE_IMGUI
	if (!open_) {
		return command;
	}
	const auto text = [language](const char* english) {
		return editor::Localize(language, english);
	};
	if (!ImGui::Begin(text("Scene Visibility###Visibility & Cull"), &open_)) {
		ImGui::End();
		return command;
	}

	ImGui::Text("%s: %d / 11", text("Visible"), CountVisibleItems(command.value));
	const bool debugVisible =
		command.value.showSkeleton ||
		command.value.showLevelGizmos ||
		command.value.showCollisionGizmos;
	if (debugVisible) {
		ImGui::TextColored(
			ImVec4(1.0f, 0.72f, 0.2f, 1.0f),
			"%s",
			text("Debug visualizations are visible."));
	} else {
		ImGui::TextDisabled("%s", text("Debug visualizations are hidden."));
	}

	ImGui::SeparatorText(text("Quick Presets"));
	if (ImGui::BeginTable("##VisibilityPresets", 2, ImGuiTableFlags_SizingStretchSame)) {
		ImGui::TableNextColumn();
		if (ImGui::Button(
			text("Show All###VisibilityShowAll"),
			ImVec2(ImGui::GetContentRegionAvail().x, 0.0f))) {
			ShowAll(command.value);
			command.apply = true;
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("%s", text("Show every Scene object and debug visualization."));
		}

		ImGui::TableNextColumn();
		if (ImGui::Button(
			text("Hide Debug###VisibilityHideDebug"),
			ImVec2(ImGui::GetContentRegionAvail().x, 0.0f))) {
			HideDebugVisualizations(command.value);
			command.apply = true;
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("%s", text("Keep Scene objects visible and hide only debug helpers."));
		}

		ImGui::TableNextColumn();
		if (ImGui::Button(
			text("Farm Editing###VisibilityFarmEditing"),
			ImVec2(ImGui::GetContentRegionAvail().x, 0.0f))) {
			ApplyFarmEditingPreset(command.value);
			command.apply = true;
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("%s", text("Show the Farm, Level objects, and effects without test objects."));
		}

		ImGui::TableNextColumn();
		if (ImGui::Button(
			text("Clean Capture###VisibilityCleanCapture"),
			ImVec2(ImGui::GetContentRegionAvail().x, 0.0f))) {
			ApplyCleanCapturePreset(command.value);
			command.apply = true;
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("%s", text("Show only the Farm, HUD, and effect particles for recording."));
		}
		ImGui::EndTable();
	}
	if (ImGui::Button(
		text("Reset###VisibilityReset"),
		ImVec2(ImGui::GetContentRegionAvail().x, 0.0f))) {
		ResetVisibility(command.value);
		command.apply = true;
	}

	ImGui::SeparatorText(text("Editing"));
	constexpr const char* kTargets[] = { "None", "Sprite", "Object3D", "Particle", "Sphere" };
	command.apply |= ImGui::Combo(
		text("Edit Focus"), &command.value.selectedTarget, kTargets, 5);

	ImGui::SeparatorText(text("Scene Objects"));
	command.apply |= ImGui::Checkbox(text("Terrain"), &command.value.showTerrain);
	command.apply |= ImGui::Checkbox(text("Sphere"), &command.value.showSphere);
	command.apply |= ImGui::Checkbox(text("Plane"), &command.value.showPlane);
	command.apply |= ImGui::Checkbox(text("Sprite (2D)"), &command.value.showSprite);
	command.apply |= ImGui::Checkbox(text("Particles"), &command.value.showParticles);
	command.apply |= ImGui::Checkbox(
		text("Animated Model"), &command.value.showAnimatedModel);
	command.apply |= ImGui::Checkbox(
		text("Level Objects"), &command.value.showLevelObjects);

	ImGui::SeparatorText(text("Farm Visualization"));
	command.apply |= ImGui::Checkbox(text("Farm placeholder meshes"), &command.value.showFarmMeshes);
	ImGui::Text(text("Farm mesh parts: %d"), viewData.farmMeshPartCount);
	if (!viewData.farmMeshesReady || viewData.farmMeshLimitExceeded) {
		ImGui::TextWrapped("%s", text("Farm meshes unavailable or over limit; debug lines remain."));
	}
	ImGui::TextDisabled("%s", text("Farm grid and tile selection stay visible."));

	ImGui::SeparatorText(text("Debug Gizmos"));
	command.apply |= ImGui::Checkbox(
		text("Skeleton Debug"), &command.value.showSkeleton);
	command.apply |= ImGui::Checkbox(
		text("Level Gizmos"), &command.value.showLevelGizmos);
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip(
			"%s",
			text("Level object bounds, routes, cameras, lights, and trigger helpers."));
	}
	command.apply |= ImGui::Checkbox(
		text("Collision Gizmos"), &command.value.showCollisionGizmos);
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip(
			"%s",
			text("Runtime collider bounds. This includes the large cyan debug box."));
	}

	if (ImGui::CollapsingHeader(text("Advanced Rendering"))) {
		constexpr const char* kCullModes[] = { "None (Two-sided)", "Front", "Back" };
		command.apply |= ImGui::Combo(
			text("Cull Mode"), &command.value.cullMode, kCullModes, 3);
	}
	ImGui::End();
#else
	(void)viewData;
	(void)language;
#endif
	return command;
}

void VisibilityWindow::SetOpen(bool open) noexcept {
	open_ = open;
}

bool VisibilityWindow::IsOpen() const noexcept {
	return open_;
}
