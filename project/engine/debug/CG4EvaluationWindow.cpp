#include "debug/CG4EvaluationWindow.h"

#include "3d/Skeleton.h"
#include "externals/imgui/imgui.h"

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <iterator>

namespace {
constexpr const char* kAnimationModelNames[] = {
	"AnimatedCube",
	"simpleSkin",
	"human / walk",
	"human / sneakWalk",
};
constexpr float kRadiansToDegrees = 57.29577951308232f;
constexpr float kDegreesToRadians = 0.017453292519943295f;

void DrawStatusRow(const char* label, bool ready, const char* readyText, const char* missingText) {
	ImGui::TableNextRow();
	ImGui::TableSetColumnIndex(0);
	ImGui::TextUnformatted(label);
	ImGui::TableSetColumnIndex(1);
	const ImVec4 color = ready
		? ImVec4(0.30f, 0.90f, 0.45f, 1.0f)
		: ImVec4(1.00f, 0.45f, 0.25f, 1.0f);
	ImGui::TextColored(color, "%s", ready ? readyText : missingText);
}

bool IsValidJointIndex(const Skeleton& skeleton, int32_t jointIndex) {
	return jointIndex >= 0 && jointIndex < static_cast<int32_t>(skeleton.joints.size());
}

bool IsJointAncestor(const Skeleton& skeleton, int32_t ancestorIndex, int32_t jointIndex) {
	if (!IsValidJointIndex(skeleton, ancestorIndex) || !IsValidJointIndex(skeleton, jointIndex)) {
		return false;
	}
	int32_t currentIndex = jointIndex;
	for (size_t depth = 0; depth < skeleton.joints.size(); ++depth) {
		if (currentIndex == ancestorIndex) {
			return true;
		}
		const std::optional<int32_t>& parent = skeleton.joints[static_cast<size_t>(currentIndex)].parent;
		if (!parent.has_value() || !IsValidJointIndex(skeleton, *parent)) {
			break;
		}
		currentIndex = *parent;
	}
	return false;
}

void DrawJointHierarchyNode(
	const Skeleton& skeleton,
	int32_t jointIndex,
	int32_t selectedJointIndex,
	std::optional<int32_t>& requestedJointIndex) {
	if (!IsValidJointIndex(skeleton, jointIndex)) {
		return;
	}
	const Joint& joint = skeleton.joints[static_cast<size_t>(jointIndex)];
	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
	if (joint.children.empty()) {
		flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
	}
	if (jointIndex == selectedJointIndex) {
		flags |= ImGuiTreeNodeFlags_Selected;
	}
	if (IsJointAncestor(skeleton, jointIndex, selectedJointIndex)) {
		ImGui::SetNextItemOpen(true, ImGuiCond_Once);
	}
	const bool open = ImGui::TreeNodeEx(
		reinterpret_cast<void*>(static_cast<intptr_t>(jointIndex + 1)),
		flags,
		"%s",
		joint.name.c_str());
	if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
		requestedJointIndex = jointIndex;
	}
	if (open && !joint.children.empty()) {
		for (int32_t childIndex : joint.children) {
			DrawJointHierarchyNode(skeleton, childIndex, selectedJointIndex, requestedJointIndex);
		}
		ImGui::TreePop();
	}
}
} // namespace

std::optional<bool> CG4EvaluationWindow::DrawModeBar(bool evaluationMode) {
	std::optional<bool> requestedMode;
	if (!ImGui::BeginMainMenuBar()) {
		return requestedMode;
	}

	ImGui::TextColored(ImVec4(0.35f, 0.80f, 1.0f, 1.0f), "CG4 評価課題");
	ImGui::Separator();
	ImGui::TextUnformatted("表示モード:");
	ImGui::SameLine();
	if (ImGui::Selectable("CG4評価用UI", evaluationMode, ImGuiSelectableFlags_DontClosePopups, ImVec2(112.0f, 0.0f))) {
		requestedMode = true;
	}
	ImGui::SameLine();
	if (ImGui::Selectable("従来デバッグUI", !evaluationMode, ImGuiSelectableFlags_DontClosePopups, ImVec2(132.0f, 0.0f))) {
		requestedMode = false;
	}
	ImGui::SameLine();
	ImGui::TextDisabled("| F1: Debug表示 / Play表示 | 上のボタンで評価UIを切替");
	ImGui::EndMainMenuBar();
	return requestedMode;
}

CG4EvaluationActions CG4EvaluationWindow::Draw(const CG4EvaluationViewData& viewData) {
	CG4EvaluationActions actions;
	ImGui::SetNextWindowSize(ImVec2(480.0f, 780.0f), ImGuiCond_FirstUseEver);
	// 表示名は日本語のまま、既存のCG4 Dockレイアウト用IDを維持する。
	if (!ImGui::Begin("CG4 評価課題###CG4 Evaluation")) {
		ImGui::End();
		return actions;
	}

	ImGui::TextColored(ImVec4(0.35f, 0.80f, 1.0f, 1.0f), "評価デモ・プリセット");
	if (ImGui::Button("ゲーム画面", ImVec2(104.0f, 0.0f))) {
		actions.preset = CG4EvaluationPreset::Gameplay;
	}
	ImGui::SameLine();
	if (ImGui::Button("スキニング", ImVec2(104.0f, 0.0f))) {
		actions.preset = CG4EvaluationPreset::Skinning;
	}
	ImGui::SameLine();
	if (ImGui::Button("骨デバッグ", ImVec2(104.0f, 0.0f))) {
		actions.preset = CG4EvaluationPreset::Skeleton;
	}
	if (ImGui::Button("MultiMesh + MultiMaterial", ImVec2(226.0f, 0.0f))) {
		actions.preset = CG4EvaluationPreset::MultiMeshMaterial;
	}
	ImGui::SameLine();
	if (ImGui::Button("左手 水エフェクト", ImVec2(152.0f, 0.0f))) {
		actions.preset = CG4EvaluationPreset::GpuParticle;
	}
	if (ImGui::Button("右手武器", ImVec2(104.0f, 0.0f))) {
		actions.preset = CG4EvaluationPreset::WeaponAttachment;
	}

	ImGui::SeparatorText("評価項目の準備状況");
	if (ImGui::BeginTable("CG4RubricReadiness", 2, ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp)) {
		ImGui::TableSetupColumn("評価項目", ImGuiTableColumnFlags_WidthStretch, 1.8f);
		ImGui::TableSetupColumn("現在の状態", ImGuiTableColumnFlags_WidthStretch, 1.0f);
		DrawStatusRow("Skinning Model", viewData.hasSkinCluster, "READY", "NO SKIN");
		DrawStatusRow("ComputeShader Skinning", viewData.hasSkinCluster && viewData.computeSkinningEnabled, "READY", "DISABLED");
		DrawStatusRow("MultiMesh / MultiMaterial", viewData.multiMeshMaterialSampleReady, "READY", "SAMPLE UNAVAILABLE");
		DrawStatusRow("Skeleton Gizmo", viewData.skeleton != nullptr, "READY", "NO SKELETON");
		DrawStatusRow("GPU Particle", viewData.gpuParticleAvailable, "READY", "UNAVAILABLE");
		DrawStatusRow(
			"Animation Cross-fade",
			viewData.animationCrossFadeAvailable,
			viewData.animationCrossFadeActive ? "ACTIVE" : "READY",
			"UNAVAILABLE");
		DrawStatusRow(
			"LeftHand Water GPU Particle",
			viewData.handParticleAttachmentReady && viewData.handParticleAttachmentActive,
			"ACTIVE",
			viewData.handParticleAttachmentReady ? "NOT ACTIVE" : "JOINT UNAVAILABLE");
		DrawStatusRow(
			"Weapon Attachment",
			viewData.weaponAttachmentReady && (!viewData.showWeapon || viewData.weaponAttachmentActive),
			viewData.showWeapon ? "ACTIVE" : "READY",
			viewData.weaponAttachmentReady ? "NOT ACTIVE" : "JOINT UNAVAILABLE");
		ImGui::EndTable();
	}

	if (viewData.showMultiMeshMaterialSample) {
		ImGui::SeparatorText("MultiMesh / MultiMaterial");
		ImGui::TextUnformatted("表示アセット: Resources/multiMaterial.obj");
		ImGui::TextColored(
			viewData.multiMeshMaterialSampleReady
				? ImVec4(0.30f, 0.90f, 0.45f, 1.0f)
				: ImVec4(1.00f, 0.45f, 0.25f, 1.0f),
			"%u Mesh / %u Material - %s",
			viewData.meshCount,
			viewData.materialCount,
			viewData.multiMeshMaterialSampleReady ? "READY" : "INVALID");
		ImGui::TextWrapped("PlaneとCubeを別Mesh・別Materialとして、1つのModelから描画します。");
	}

	ImGui::SeparatorText("右手武器 Socket");
	bool showWeapon = viewData.showWeapon;
	if (ImGui::Checkbox("BusterSwordを表示", &showWeapon)) {
		actions.showWeapon = showWeapon;
	}
	ImGui::SameLine();
	bool showWeaponGizmo = viewData.showWeaponGizmo;
	if (ImGui::Checkbox("武器Socket Gizmo", &showWeaponGizmo)) {
		actions.showWeaponGizmo = showWeaponGizmo;
	}
	ImGui::Text("追従Bone: mixamorig:RightHand");
	ImGui::Text(
		"Bone Scale: %.4f, %.4f, %.4f",
		viewData.weaponSocketScale.x,
		viewData.weaponSocketScale.y,
		viewData.weaponSocketScale.z);
	ImGui::TextColored(
		viewData.weaponAttachmentActive
			? ImVec4(0.30f, 0.90f, 0.45f, 1.0f)
			: ImVec4(1.00f, 0.65f, 0.25f, 1.0f),
		"状態: %s | %uV / %uI / %u Mesh / %u Material",
		viewData.weaponAttachmentActive ? "ACTIVE" : "INACTIVE",
		viewData.weaponVertexCount,
		viewData.weaponIndexCount,
		viewData.weaponMeshCount,
		viewData.weaponMaterialCount);

	Transform weaponTransform = viewData.weaponLocalTransform;
	bool weaponTransformChanged = false;
	weaponTransformChanged |= ImGui::DragFloat3(
		"武器 Local Position (Bone軸)", &weaponTransform.translate.x, 0.01f, -5.0f, 5.0f, "%.3f");
	Vector3 rotationDegrees = {
		weaponTransform.rotate.x * kRadiansToDegrees,
		weaponTransform.rotate.y * kRadiansToDegrees,
		weaponTransform.rotate.z * kRadiansToDegrees,
	};
	if (ImGui::DragFloat3("武器 Local Rotation (deg)", &rotationDegrees.x, 1.0f, -180.0f, 180.0f, "%.1f")) {
		weaponTransform.rotate = {
			rotationDegrees.x * kDegreesToRadians,
			rotationDegrees.y * kDegreesToRadians,
			rotationDegrees.z * kDegreesToRadians,
		};
		weaponTransformChanged = true;
	}
	float uniformScale = weaponTransform.scale.x;
	if (ImGui::DragFloat("武器 Uniform Scale", &uniformScale, 0.001f, 0.001f, 2.000f, "%.3f")) {
		weaponTransform.scale = { uniformScale, uniformScale, uniformScale };
		weaponTransformChanged = true;
	}
	if (weaponTransformChanged) {
		actions.weaponLocalTransform = weaponTransform;
	}
	if (ImGui::Button("武器位置をリセット")) {
		actions.resetWeaponTransform = true;
	}

	ImGui::SeparatorText("左手 GPU Particle Socket");
	bool showHandParticleGizmo = viewData.showHandParticleGizmo;
	if (ImGui::Checkbox("Particle放出位置Gizmo", &showHandParticleGizmo)) {
		actions.showHandParticleGizmo = showHandParticleGizmo;
	}
	ImGui::Text("追従Bone: mixamorig:LeftHand");
	ImGui::Text(
		"World Position: %.3f, %.3f, %.3f",
		viewData.handParticleWorldPosition.x,
		viewData.handParticleWorldPosition.y,
		viewData.handParticleWorldPosition.z);
	ImGui::Text(
		"Bone Scale: %.4f, %.4f, %.4f",
		viewData.handParticleSocketScale.x,
		viewData.handParticleSocketScale.y,
		viewData.handParticleSocketScale.z);
	ImGui::TextColored(
		viewData.handParticleAttachmentActive
			? ImVec4(0.30f, 0.90f, 0.45f, 1.0f)
			: ImVec4(1.00f, 0.65f, 0.25f, 1.0f),
		"状態: %s",
		viewData.handParticleAttachmentActive ? "LEFT HAND READY" : "INACTIVE");
	Transform particleTransform = viewData.handParticleLocalTransform;
	if (ImGui::DragFloat3(
		"Particle Local Position (Bone軸)",
		&particleTransform.translate.x,
		0.005f,
		-2.0f,
		2.0f,
		"%.3f")) {
		actions.handParticleLocalTransform = particleTransform;
	}
	if (ImGui::Button("Particle位置をリセット")) {
		actions.resetHandParticleTransform = true;
	}
	ImGui::SameLine();
	if (!viewData.handParticleAttachmentActive) {
		ImGui::BeginDisabled();
	}
	if (ImGui::Button("左手から小さい水流を放出")) {
		actions.emitGpuParticleSample = true;
	}
	if (!viewData.handParticleAttachmentActive) {
		ImGui::EndDisabled();
	}

	ImGui::SeparatorText("Skinning / Animation");
	ImGui::TextColored(
		viewData.animationSkinningIsolated
			? ImVec4(0.30f, 0.90f, 0.45f, 1.0f)
			: ImVec4(1.00f, 0.65f, 0.25f, 1.0f),
		"SkinCluster Instance: %s",
		viewData.animationSkinningIsolated ? "ISOLATED" : "SHARED / UNAVAILABLE");
	if (viewData.showMultiMeshMaterialSample) {
		ImGui::TextColored(ImVec4(0.30f, 0.90f, 0.45f, 1.0f), "MultiMesh + MultiMaterialサンプル表示中");
	}
	int modelIndex = std::clamp(viewData.animationModelIndex, 0, static_cast<int32_t>(std::size(kAnimationModelNames)) - 1);
	if (ImGui::Combo("Model", &modelIndex, kAnimationModelNames, static_cast<int>(std::size(kAnimationModelNames)))) {
		actions.animationModelIndex = modelIndex;
	}
	float crossFadeDuration = viewData.animationCrossFadeDuration;
	if (ImGui::SliderFloat("補間時間 (秒)", &crossFadeDuration, 0.05f, 1.0f, "%.2f")) {
		actions.animationCrossFadeDuration = crossFadeDuration;
	}
	if (!viewData.animationCrossFadeAvailable) {
		ImGui::BeginDisabled();
	}
	if (ImGui::Button("Walkへ補間")) {
		actions.crossFadeToWalk = true;
	}
	ImGui::SameLine();
	if (ImGui::Button("Sneakへ補間")) {
		actions.crossFadeToSneak = true;
	}
	if (!viewData.animationCrossFadeAvailable) {
		ImGui::EndDisabled();
	}
	ImGui::SameLine();
	ImGui::Text(
		"%s %.0f%%",
		viewData.animationCrossFadeActive ? "補間中" : "待機",
		std::clamp(viewData.animationCrossFadeProgress, 0.0f, 1.0f) * 100.0f);

	bool showModel = viewData.showAnimatedModel;
	if (ImGui::Checkbox("Show Model", &showModel)) {
		actions.showAnimatedModel = showModel;
	}
	ImGui::SameLine();
	bool showSkeleton = viewData.showSkeleton;
	if (ImGui::Checkbox("Show Skeleton", &showSkeleton)) {
		actions.showSkeleton = showSkeleton;
	}

	bool useCompute = viewData.computeSkinningEnabled;
	if (!viewData.hasSkinCluster) {
		ImGui::BeginDisabled();
	}
	if (ImGui::Checkbox("ComputeShader Skinning", &useCompute)) {
		actions.computeSkinningEnabled = useCompute;
	}
	if (!viewData.hasSkinCluster) {
		ImGui::EndDisabled();
	}

	bool playing = viewData.animationPlaying;
	if (ImGui::Checkbox("Play Animation", &playing)) {
		actions.animationPlaying = playing;
	}
	ImGui::SameLine();
	if (ImGui::Button("Reset Time")) {
		actions.resetAnimation = true;
	}

	float speed = viewData.animationSpeed;
	if (ImGui::SliderFloat("Playback Speed", &speed, -2.0f, 2.0f, "%.2f")) {
		actions.animationSpeed = speed;
	}
	if (viewData.animationDuration > 0.0f) {
		float time = std::clamp(viewData.animationTime, 0.0f, viewData.animationDuration);
		if (ImGui::SliderFloat("Timeline", &time, 0.0f, viewData.animationDuration, "%.3f s")) {
			actions.animationTime = time;
		}
	}

	ImGui::SeparatorText("Skeleton Selection");
	bool showAxes = viewData.showLocalAxes;
	if (ImGui::Checkbox("選択Joint RGB軸", &showAxes)) {
		actions.showLocalAxes = showAxes;
	}
	ImGui::SameLine();
	bool showAllAxes = viewData.showAllLocalAxes;
	if (ImGui::Checkbox("全Joint RGB軸", &showAllAxes)) {
		actions.showAllLocalAxes = showAllAxes;
	}
	bool showJointMarkers = viewData.showJointMarkers;
	if (ImGui::Checkbox("Jointマーカー", &showJointMarkers)) {
		actions.showJointMarkers = showJointMarkers;
	}
	ImGui::SameLine();
	bool highlightSelectedChain = viewData.highlightSelectedChain;
	if (ImGui::Checkbox("選択チェーン強調", &highlightSelectedChain)) {
		actions.highlightSelectedChain = highlightSelectedChain;
	}
	ImGui::TextColored(ImVec4(0.25f, 1.00f, 0.35f, 1.0f), "Root");
	ImGui::SameLine();
	ImGui::TextColored(ImVec4(0.25f, 0.85f, 1.00f, 1.0f), "通常Joint");
	ImGui::SameLine();
	ImGui::TextColored(ImVec4(1.00f, 0.25f, 0.80f, 1.0f), "選択チェーン");
	ImGui::SameLine();
	ImGui::TextColored(ImVec4(1.00f, 0.75f, 0.10f, 1.0f), "選択Joint");
	ImGui::TextColored(ImVec4(1.00f, 0.92f, 0.05f, 1.0f), "黄色の太枠 = 選択Bone（親Jointから選択Joint）");
	const Skeleton* skeleton = viewData.skeleton;
	const char* selectedJointName = "No Joint";
	if (skeleton && viewData.selectedJointIndex >= 0 &&
		viewData.selectedJointIndex < static_cast<int32_t>(skeleton->joints.size())) {
		selectedJointName = skeleton->joints[viewData.selectedJointIndex].name.c_str();
	}
	if (ImGui::BeginCombo("Selected Joint", selectedJointName)) {
		if (skeleton) {
			for (const Joint& joint : skeleton->joints) {
				const bool selected = joint.index == viewData.selectedJointIndex;
				if (ImGui::Selectable(joint.name.c_str(), selected)) {
					actions.selectedJointIndex = joint.index;
				}
				if (selected) {
					ImGui::SetItemDefaultFocus();
				}
			}
		}
		ImGui::EndCombo();
	}
	ImGui::TextUnformatted("Joint階層（名前をクリックして選択）");
	if (ImGui::BeginChild("CG4JointHierarchy", ImVec2(0.0f, 180.0f), true)) {
		if (skeleton && IsValidJointIndex(*skeleton, skeleton->root)) {
			DrawJointHierarchyNode(
				*skeleton,
				skeleton->root,
				viewData.selectedJointIndex,
				actions.selectedJointIndex);
		} else {
			ImGui::TextDisabled("Skeletonがありません");
		}
	}
	ImGui::EndChild();

	if (skeleton && IsValidJointIndex(*skeleton, viewData.selectedJointIndex)) {
		const Joint& selectedJoint = skeleton->joints[static_cast<size_t>(viewData.selectedJointIndex)];
		const char* parentName = "None (Root)";
		if (selectedJoint.parent.has_value() && IsValidJointIndex(*skeleton, *selectedJoint.parent)) {
			parentName = skeleton->joints[static_cast<size_t>(*selectedJoint.parent)].name.c_str();
		}
		ImGui::SeparatorText("選択Joint 詳細");
		ImGui::Text("Name: %s", selectedJoint.name.c_str());
		ImGui::Text("Index: %d | Parent: %s | Children: %zu", selectedJoint.index, parentName, selectedJoint.children.size());
		ImGui::Text(
			"Local Position: %.3f, %.3f, %.3f",
			selectedJoint.transform.translate.x,
			selectedJoint.transform.translate.y,
			selectedJoint.transform.translate.z);
		ImGui::Text(
			"Local Rotation Quaternion: %.3f, %.3f, %.3f, %.3f",
			selectedJoint.transform.rotate.x,
			selectedJoint.transform.rotate.y,
			selectedJoint.transform.rotate.z,
			selectedJoint.transform.rotate.w);
		ImGui::Text(
			"Local Scale: %.3f, %.3f, %.3f",
			selectedJoint.transform.scale.x,
			selectedJoint.transform.scale.y,
			selectedJoint.transform.scale.z);
		ImGui::Text(
			"Skeleton Position: %.3f, %.3f, %.3f",
			selectedJoint.skeletonSpaceMatrix.m[3][0],
			selectedJoint.skeletonSpaceMatrix.m[3][1],
			selectedJoint.skeletonSpaceMatrix.m[3][2]);
		if (!selectedJoint.children.empty()) {
			ImGui::TextUnformatted("Children:");
			for (int32_t childIndex : selectedJoint.children) {
				if (IsValidJointIndex(*skeleton, childIndex)) {
					ImGui::BulletText("%s", skeleton->joints[static_cast<size_t>(childIndex)].name.c_str());
				}
			}
		}
	}

	ImGui::SeparatorText("現在の描画データ");
	ImGui::Text("Vertices : %u", viewData.vertexCount);
	ImGui::Text("Indices  : %u", viewData.indexCount);
	ImGui::Text("Meshes   : %u", viewData.meshCount);
	ImGui::Text("Materials: %u", viewData.materialCount);
	ImGui::Text("Joints   : %u", viewData.jointCount);
	ImGui::Text("GPU Mode : %d", viewData.gpuParticleMode);

	ImGui::SeparatorText("Display");
	bool showParticles = viewData.showParticles;
	if (ImGui::Checkbox("Show Particles", &showParticles)) {
		actions.showParticles = showParticles;
	}
	ImGui::End();
	return actions;
}
