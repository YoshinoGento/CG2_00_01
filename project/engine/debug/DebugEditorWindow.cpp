#include "debug/DebugEditorWindow.h"

#include "3d/Model.h"
#include "3d/Object3d.h"
#include "externals/imgui/imgui.h"

#include <algorithm>

namespace {
struct DebugEditorPaletteItem {
	const char* name = "";
	const char* shortName = "";
	const char* type = "";
	const char* description = "";
	DebugEditorWindow::SpawnKind kind = DebugEditorWindow::SpawnKind::StaticModel;
	const char* modelPath = "";
	const char* animationDirectory = "";
	const char* animationFile = "";
};

constexpr DebugEditorPaletteItem kPaletteItems[] = {
	{ "Cube.obj", "CUBE", "OBJ", "Static cube test object", DebugEditorWindow::SpawnKind::StaticModel, "tl1/Cube.obj", "", "" },
	{ "Sphere", "SPHR", "PRIM", "Generated sphere primitive", DebugEditorWindow::SpawnKind::SpherePrimitive, "", "", "" },
	{ "Plane.obj", "PLN", "OBJ", "Static plane test object", DebugEditorWindow::SpawnKind::StaticModel, "plane.obj", "", "" },
	{ "simpleSkin.gltf", "SKIN", "ANIM", "Skinned sample model", DebugEditorWindow::SpawnKind::AnimatedModel, "simpleSkin/simpleSkin.gltf", "simpleSkin", "simpleSkin.gltf" },
	{ "human/walk.gltf", "HUMN", "ANIM", "Human walk animation", DebugEditorWindow::SpawnKind::AnimatedModel, "human/walk.gltf", "human", "walk.gltf" },
};
constexpr int32_t kPaletteItemCount = static_cast<int32_t>(sizeof(kPaletteItems) / sizeof(kPaletteItems[0]));
}

void DebugEditorWindow::ClearTargets() {
	targets_.clear();
}

void DebugEditorWindow::AddTarget(const char* name, Object3d* object) {
	if (!object) {
		return;
	}
	targets_.push_back({ name ? name : "(unnamed)", object });
}

void DebugEditorWindow::Draw(bool* showSceneDebugWindow, bool* showFarmEditorWindow) {
	ClampSelection();

	if (ImGui::Begin("Debug Editor")) {
		DrawToolbar(showSceneDebugWindow, showFarmEditorWindow);
		DrawMainLayout();
	}
	ImGui::End();
}

Object3d* DebugEditorWindow::GetSelectedObject() const {
	const Target* target = GetSelectedTarget();
	return target ? target->object : nullptr;
}

bool DebugEditorWindow::ConsumeSpawnRequest(SpawnRequest& outRequest) {
	if (!hasPendingSpawnRequest_) {
		return false;
	}
	outRequest = pendingSpawnRequest_;
	hasPendingSpawnRequest_ = false;
	return true;
}

void DebugEditorWindow::SelectObject(Object3d* object) {
	if (!object) {
		selectedTargetIndex_ = -1;
		return;
	}

	for (int32_t index = 0; index < static_cast<int32_t>(targets_.size()); ++index) {
		if (targets_[index].object == object) {
			selectedTargetIndex_ = index;
			return;
		}
	}
}

void DebugEditorWindow::DrawToolbar(bool* showSceneDebugWindow, bool* showFarmEditorWindow) {
	if (!ImGui::BeginChild("DebugEditorToolbar", ImVec2(0.0f, 72.0f), true, ImGuiWindowFlags_NoScrollbar)) {
		ImGui::EndChild();
		return;
	}

	ImGui::TextUnformatted("Godot-Style Debug Editor");
	ImGui::SameLine();
	ImGui::TextDisabled("|");
	ImGui::SameLine();
	DrawModeButton("Select", EditorMode::Select);
	ImGui::SameLine();
	DrawModeButton("Place", EditorMode::Place);
	ImGui::SameLine();
	DrawModeButton("Animation", EditorMode::Animation);
	ImGui::SameLine();
	DrawModeButton("Light", EditorMode::Light);
	ImGui::SameLine();
	DrawModeButton("Particle", EditorMode::Particle);
	ImGui::SameLine();
	DrawModeButton("Farm", EditorMode::Farm);

	ImGui::Separator();
	ImGui::Checkbox("Object Selection", &objectSelectionModeEnabled_);
	ImGui::SameLine();
	if (ImGui::Button("Clear")) {
		selectedTargetIndex_ = -1;
	}
	ImGui::SameLine();
	const Target* selected = GetSelectedTarget();
	ImGui::Text("Selected: %s", selected ? selected->name.c_str() : "(none)");

	ImGui::SameLine();
	ImGui::Checkbox("Advanced", &showAdvanced_);
	if (showAdvanced_) {
		if (showSceneDebugWindow) {
			ImGui::SameLine();
			ImGui::Checkbox("Scene", showSceneDebugWindow);
		}
		if (showFarmEditorWindow) {
			ImGui::SameLine();
			ImGui::Checkbox("Farm", showFarmEditorWindow);
		}
	}

	ImGui::EndChild();
}

void DebugEditorWindow::DrawModeButton(const char* label, EditorMode mode) {
	const bool selected = editorMode_ == mode;
	if (selected) {
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.30f, 0.48f, 0.68f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.36f, 0.56f, 0.78f, 1.0f));
	}

	if (ImGui::Button(label, ImVec2(92.0f, 0.0f))) {
		editorMode_ = mode;
	}

	if (selected) {
		ImGui::PopStyleColor(2);
	}
}

void DebugEditorWindow::DrawMainLayout() {
	if (!ImGui::BeginTable("DebugEditorLayout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV, ImVec2(0.0f, 0.0f))) {
		return;
	}

	ImGui::TableSetupColumn("Left", ImGuiTableColumnFlags_WidthFixed, 280.0f);
	ImGui::TableSetupColumn("Inspector", ImGuiTableColumnFlags_WidthStretch);
	ImGui::TableNextRow();

	ImGui::TableSetColumnIndex(0);
	if (ImGui::BeginChild("DebugEditorLeftPanel", ImVec2(0.0f, 0.0f), true)) {
		DrawLeftPanel();
	}
	ImGui::EndChild();

	ImGui::TableSetColumnIndex(1);
	if (ImGui::BeginChild("DebugEditorInspectorPanel", ImVec2(0.0f, 0.0f), true)) {
		DrawInspectorPanel();
	}
	ImGui::EndChild();

	ImGui::EndTable();
}

void DebugEditorWindow::DrawLeftPanel() {
	switch (editorMode_) {
	case EditorMode::Place:
		DrawObjectPalette();
		break;
	case EditorMode::Animation:
		DrawObjectList(true);
		break;
	case EditorMode::Light:
		ImGui::TextUnformatted("Light List");
		ImGui::Separator();
		ImGui::BulletText("Directional Light");
		ImGui::BulletText("Spot Light");
		break;
	case EditorMode::Particle:
		ImGui::TextUnformatted("Particle Tools");
		ImGui::Separator();
		ImGui::BulletText("Manual Emit");
		ImGui::BulletText("GPU Particle");
		ImGui::BulletText("Interaction Brush");
		break;
	case EditorMode::Farm:
		ImGui::TextUnformatted("Farm");
		ImGui::Separator();
		ImGui::BulletText("FarmGrid");
		ImGui::BulletText("Selected Tile");
		ImGui::BulletText("Tool Action");
		break;
	case EditorMode::Select:
	default:
		DrawObjectList(false);
		break;
	}
}

void DebugEditorWindow::DrawInspectorPanel() {
	const Target* target = GetSelectedTarget();
	Object3d* object = target ? target->object : nullptr;

	switch (editorMode_) {
	case EditorMode::Place:
		selectedPaletteIndex_ = std::clamp(selectedPaletteIndex_, 0, kPaletteItemCount - 1);
		ImGui::TextUnformatted("Spawn Settings");
		ImGui::Separator();
		ImGui::Text("Candidate : %s", kPaletteItems[selectedPaletteIndex_].name);
		ImGui::Text("Type      : %s", kPaletteItems[selectedPaletteIndex_].type);
		ImGui::TextWrapped("%s", kPaletteItems[selectedPaletteIndex_].description);
		if (ImGui::Button("Spawn Selected")) {
			RequestSpawnSelectedPaletteItem();
		}
		ImGui::TextDisabled("Clicking a palette card also spawns one debug object.");
		break;
	case EditorMode::Animation:
		DrawAnimationInspector(object, target);
		break;
	case EditorMode::Light:
		DrawLightInspector();
		break;
	case EditorMode::Particle:
		DrawParticleInspector();
		break;
	case EditorMode::Farm:
		DrawFarmInspector(nullptr);
		break;
	case EditorMode::Select:
	default:
		DrawObjectInspector(object, target);
		break;
	}
}

void DebugEditorWindow::DrawObjectList(bool animationOnly) {
	ImGui::TextUnformatted(animationOnly ? "Animated Objects" : "Scene Objects");
	ImGui::Separator();

	if (!objectSelectionModeEnabled_) {
		ImGui::TextDisabled("Object Selection is OFF.");
	}

	for (int32_t index = 0; index < static_cast<int32_t>(targets_.size()); ++index) {
		const Target& target = targets_[index];
		if (!target.object) {
			continue;
		}
		if (animationOnly && !target.object->HasAnimation()) {
			continue;
		}

		std::string label = target.object->HasAnimation() ? "[ANIM] " : "[OBJ] ";
		label += target.name;
		if (target.object->HasSkinning()) {
			label += " [SKIN]";
		}

		if (!objectSelectionModeEnabled_) {
			ImGui::BeginDisabled();
		}
		if (ImGui::Selectable(label.c_str(), selectedTargetIndex_ == index) && objectSelectionModeEnabled_) {
			SelectTarget(index);
		}
		if (!objectSelectionModeEnabled_) {
			ImGui::EndDisabled();
		}
	}
}

void DebugEditorWindow::DrawObjectPalette() {
	ImGui::TextUnformatted("Object Palette");
	ImGui::Separator();

	constexpr float cardWidth = 112.0f;
	constexpr float thumbnailHeight = 64.0f;
	constexpr float labelHeight = 28.0f;
	const float spacing = ImGui::GetStyle().ItemSpacing.x;
	const float availableWidth = ImGui::GetContentRegionAvail().x;
	const int32_t columns = (std::max)(1, static_cast<int32_t>((availableWidth + spacing) / (cardWidth + spacing)));

	for (int32_t index = 0; index < kPaletteItemCount; ++index) {
		const DebugEditorPaletteItem& item = kPaletteItems[index];
		ImGui::PushID(index);
		const bool selected = selectedPaletteIndex_ == index;
		if (selected) {
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.38f, 0.56f, 0.76f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.46f, 0.64f, 0.84f, 1.0f));
		}
		if (ImGui::Button(item.shortName, ImVec2(cardWidth, thumbnailHeight))) {
			selectedPaletteIndex_ = index;
			RequestSpawnSelectedPaletteItem();
		}
		if (selected) {
			ImGui::PopStyleColor(2);
		}
		if (ImGui::Selectable(item.name, selected, 0, ImVec2(cardWidth, labelHeight))) {
			selectedPaletteIndex_ = index;
			RequestSpawnSelectedPaletteItem();
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("%s", item.description);
		}
		if ((index + 1) % columns != 0 && index + 1 < kPaletteItemCount) {
			ImGui::SameLine();
		}
		ImGui::PopID();
	}
}

void DebugEditorWindow::DrawObjectInspector(Object3d* object, const Target* target) {
	ImGui::Text("Selected Object : %s", target ? target->name.c_str() : "(none)");
	ImGui::Text("Type            : %s", GetObjectTypeName(object));
	ImGui::Separator();

	if (!object) {
		ImGui::TextDisabled("No object selected.");
		return;
	}

	if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
		Vector3 position = object->GetPosition();
		Vector3 rotation = object->GetRotation();
		Vector3 scale = object->GetScale();
		if (ImGui::DragFloat3("Position", &position.x, 0.05f, -1000.0f, 1000.0f, "%.3f")) {
			object->SetPosition(position);
		}
		if (ImGui::DragFloat3("Rotation", &rotation.x, 0.01f, -100.0f, 100.0f, "%.3f")) {
			object->SetRotation(rotation);
		}
		if (ImGui::DragFloat3("Scale", &scale.x, 0.01f, 0.001f, 100.0f, "%.3f")) {
			object->SetScale(scale);
		}
	}

	if (ImGui::CollapsingHeader("Model", ImGuiTreeNodeFlags_DefaultOpen)) {
		const Model* model = object->GetModel();
		ImGui::Text("Vertex Count : %u", model ? model->GetVertexCount() : 0u);
		ImGui::Text("Index Count  : %u", model ? model->GetIndexCount() : 0u);
		ImGui::Text("Animation    : %s", object->HasAnimation() ? "Yes" : "No");
		ImGui::Text("Skinning     : %s", object->HasSkinning() ? "Yes" : "No");
	}

	if (showAdvanced_ && ImGui::CollapsingHeader("Advanced")) {
		ImGui::Text("Object Address: %p", static_cast<void*>(object));
		ImGui::Text("Model Address : %p", static_cast<void*>(object->GetModel()));
	}
}

void DebugEditorWindow::DrawAnimationInspector(Object3d* object, const Target* target) {
	ImGui::Text("Animated Object : %s", target ? target->name.c_str() : "(none)");
	ImGui::Separator();

	if (!object || !object->HasAnimation()) {
		ImGui::TextDisabled("No Animation.");
		return;
	}

	if (ImGui::CollapsingHeader("Animator", ImGuiTreeNodeFlags_DefaultOpen)) {
		if (ImGui::Button(object->IsAnimationPlaying() ? "Pause" : "Play")) {
			object->SetAnimationPlaying(!object->IsAnimationPlaying());
		}
		ImGui::SameLine();
		if (ImGui::Button("Reset")) {
			object->ResetAnimationTime();
		}

		float speed = object->GetPlaybackSpeed();
		if (ImGui::SliderFloat("Speed", &speed, -4.0f, 4.0f, "%.2f")) {
			object->SetPlaybackSpeed(speed);
		}

		const float duration = object->GetAnimationDuration();
		const float current = object->GetAnimationTimeValue();
		const float progress = duration > 0.0f ? std::clamp(current / duration, 0.0f, 1.0f) : 0.0f;
		ImGui::ProgressBar(progress, ImVec2(-1.0f, 20.0f));
		ImGui::Text("Time: %.3f / %.3f", current, duration);
	}

	if (ImGui::CollapsingHeader("Skinning", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Text("Skeleton : %s", object->HasSkeleton() ? "Yes" : "No");
		ImGui::Text("Skinning : %s", object->HasSkinning() ? "Yes" : "No");
		const Model* model = object->GetModel();
		ImGui::Text("Compute  : %s", model && model->UseComputeSkinning() ? "Yes" : "No");
	}

	if (showAdvanced_ && ImGui::CollapsingHeader("Advanced")) {
		ImGui::Text("Object Address: %p", static_cast<void*>(object));
		ImGui::Text("Model Address : %p", static_cast<void*>(object->GetModel()));
	}
}

void DebugEditorWindow::DrawLightInspector() {
	ImGui::TextUnformatted("Light Inspector");
	ImGui::Separator();
	ImGui::TextDisabled("Light parameters are not moved in this pass.");
	ImGui::TextDisabled("Next: route Scene light settings here.");
}

void DebugEditorWindow::DrawParticleInspector() {
	ImGui::TextUnformatted("Particle Inspector");
	ImGui::Separator();
	ImGui::TextDisabled("Particle controls are grouped conceptually here.");
	ImGui::TextDisabled("Next: move Effect Control UI into this mode.");
}

void DebugEditorWindow::DrawFarmInspector(bool*) {
	ImGui::TextUnformatted("Farm Inspector");
	ImGui::Separator();
	ImGui::TextDisabled("Farm editing remains in Farm Editor / Farm Debug.");
	ImGui::TextDisabled("Enable Advanced -> Farm to open the existing Farm window.");
}

void DebugEditorWindow::SelectTarget(int32_t index) {
	if (index < 0 || index >= static_cast<int32_t>(targets_.size())) {
		return;
	}
	selectedTargetIndex_ = index;
}

void DebugEditorWindow::RequestSpawnSelectedPaletteItem() {
	selectedPaletteIndex_ = std::clamp(selectedPaletteIndex_, 0, kPaletteItemCount - 1);
	const DebugEditorPaletteItem& item = kPaletteItems[selectedPaletteIndex_];
	pendingSpawnRequest_.kind = item.kind;
	pendingSpawnRequest_.displayName = item.name;
	pendingSpawnRequest_.modelPath = item.modelPath;
	pendingSpawnRequest_.animationDirectory = item.animationDirectory;
	pendingSpawnRequest_.animationFile = item.animationFile;
	hasPendingSpawnRequest_ = true;
}

void DebugEditorWindow::ClampSelection() {
	if (targets_.empty()) {
		selectedTargetIndex_ = -1;
		return;
	}
	if (selectedTargetIndex_ >= static_cast<int32_t>(targets_.size())) {
		selectedTargetIndex_ = -1;
	}
}

const DebugEditorWindow::Target* DebugEditorWindow::GetSelectedTarget() const {
	if (selectedTargetIndex_ < 0 || selectedTargetIndex_ >= static_cast<int32_t>(targets_.size())) {
		return nullptr;
	}
	return &targets_[selectedTargetIndex_];
}

const char* DebugEditorWindow::GetObjectTypeName(const Object3d* object) const {
	if (!object) {
		return "Unknown";
	}
	if (object->HasAnimation()) {
		return object->HasSkinning() ? "AnimatedObject + Skin" : "AnimatedObject";
	}
	return "StaticObject";
}
