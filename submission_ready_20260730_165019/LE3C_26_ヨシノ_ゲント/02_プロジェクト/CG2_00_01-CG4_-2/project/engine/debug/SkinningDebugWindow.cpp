#include "debug/SkinningDebugWindow.h"

#include "3d/Model.h"
#include "3d/Object3d.h"
#include "3d/Skeleton.h"
#include "externals/imgui/imgui.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr float kWeightEpsilon = 0.00001f;
constexpr float kWeightSumTolerance = 0.01f;

bool IsActiveInfluence(float weight)
{
	return std::fabs(weight) > kWeightEpsilon;
}

const char* GetJointName(const Skeleton* skeleton, int32_t jointIndex)
{
	if (!skeleton || jointIndex < 0 || jointIndex >= static_cast<int32_t>(skeleton->joints.size())) {
		return "(invalid)";
	}
	return skeleton->joints[jointIndex].name.c_str();
}
}

void SkinningDebugWindow::Draw(Object3d* targetObject)
{
	if (ImGui::Begin("Skinning Debug")) {
		Skeleton* skeleton = nullptr;
		if (targetObject) {
			std::optional<Skeleton>& skeletonOpt = targetObject->GetSkeleton();
			if (skeletonOpt.has_value()) {
				skeleton = &skeletonOpt.value();
			}
		}

		Model* model = targetObject ? targetObject->GetModel() : nullptr;
		ClampSelections(skeleton, model);

		DrawSummary(targetObject, skeleton, model);
		DrawErrorCheck(skeleton, model);
		DrawSkeletonInfo(skeleton);
		DrawSelectedJoint(skeleton);
		DrawVertexInfluence(skeleton, model);
		DrawMatrixPalette(targetObject, skeleton, model);
	}
	ImGui::End();
}

void SkinningDebugWindow::DrawSummary(Object3d* targetObject, const Skeleton* skeleton, const Model* model)
{
	if (ImGui::CollapsingHeader("Summary", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Text("Target Object : %s", targetObject ? "valid" : "null");
		ImGui::Text("Model         : %s", model ? "valid" : "null");
		ImGui::Text("Skeleton      : %s", skeleton ? "valid" : "null");
		ImGui::Text("SkinCluster   : %s", (model && model->HasSkinCluster()) ? "valid" : "none");
		ImGui::Text("Mapped Palette: %s", (model && model->HasSkinCluster() && model->GetSkinCluster().mappedPalette) ? "valid" : "none");
		ImGui::Separator();
		ImGui::Text("Joint Count   : %u", skeleton ? static_cast<uint32_t>(skeleton->joints.size()) : 0u);
		ImGui::Text("Influence Count: %u", GetInfluenceCount(model));
		ImGui::Text("Palette Count : %u", GetPaletteCount(model));
	}
}

void SkinningDebugWindow::DrawErrorCheck(const Skeleton* skeleton, const Model* model)
{
	if (!ImGui::CollapsingHeader("Error Check", ImGuiTreeNodeFlags_DefaultOpen)) {
		return;
	}

	ImGui::Checkbox("Show only errors", &showOnlyErrors_);
	if (ImGui::Button("Validate Skinning Data")) {
		validationResult_ = Validate(skeleton, model);
		hasValidationResult_ = true;
	}

	if (!hasValidationResult_) {
		ImGui::TextDisabled("Press Validate Skinning Data.");
		return;
	}

	const bool hasError =
		validationResult_.invalidWeightSumCount > 0 ||
		validationResult_.invalidJointIndexCount > 0 ||
		validationResult_.noInfluenceVertexCount > 0 ||
		validationResult_.paletteCountMismatch;

	const ImVec4 okColor(0.3f, 0.9f, 0.4f, 1.0f);
	const ImVec4 ngColor(1.0f, 0.25f, 0.25f, 1.0f);
	ImGui::TextColored(hasError ? ngColor : okColor, "Result: %s", hasError ? "NG" : "OK");
	ImGui::Text("Checked Vertices: %u", validationResult_.checkedVertexCount);
	ImGui::Text("Weight Sum != 1 : %u", validationResult_.invalidWeightSumCount);
	ImGui::Text("Joint Out Of Range: %u", validationResult_.invalidJointIndexCount);
	ImGui::Text("No Influence Vertex: %u", validationResult_.noInfluenceVertexCount);
	ImGui::Text("Palette Count Mismatch: %s", validationResult_.paletteCountMismatch ? "true" : "false");
}

void SkinningDebugWindow::DrawSkeletonInfo(const Skeleton* skeleton)
{
	if (!ImGui::CollapsingHeader("Skeleton", ImGuiTreeNodeFlags_DefaultOpen)) {
		return;
	}

	if (!skeleton) {
		ImGui::TextDisabled("Skeleton is null.");
		return;
	}

	ImGui::Text("Root: %d", skeleton->root);
	if (ImGui::BeginTable("JointTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0.0f, 220.0f))) {
		ImGui::TableSetupColumn("Index");
		ImGui::TableSetupColumn("Name");
		ImGui::TableSetupColumn("Parent");
		ImGui::TableSetupColumn("Children");
		ImGui::TableHeadersRow();

		for (const Joint& joint : skeleton->joints) {
			if (showOnlyErrors_) {
				continue;
			}

			const bool selected = selectedJointIndex_ == joint.index;
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			if (ImGui::Selectable(std::to_string(joint.index).c_str(), selected, ImGuiSelectableFlags_SpanAllColumns)) {
				selectedJointIndex_ = joint.index;
			}
			ImGui::TableSetColumnIndex(1);
			ImGui::TextUnformatted(joint.name.c_str());
			ImGui::TableSetColumnIndex(2);
			if (joint.parent.has_value()) {
				ImGui::Text("%d", joint.parent.value());
			} else {
				ImGui::TextUnformatted("none");
			}
			ImGui::TableSetColumnIndex(3);
			ImGui::Text("%u", static_cast<uint32_t>(joint.children.size()));
		}
		ImGui::EndTable();
	}
}

void SkinningDebugWindow::DrawSelectedJoint(const Skeleton* skeleton)
{
	if (!ImGui::CollapsingHeader("Selected Joint", ImGuiTreeNodeFlags_DefaultOpen)) {
		return;
	}

	if (!skeleton || selectedJointIndex_ < 0 || selectedJointIndex_ >= static_cast<int32_t>(skeleton->joints.size())) {
		ImGui::TextDisabled("No valid joint selected.");
		return;
	}

	const Joint& joint = skeleton->joints[selectedJointIndex_];
	ImGui::Text("Index : %d", joint.index);
	ImGui::Text("Name  : %s", joint.name.c_str());
	if (joint.parent.has_value()) {
		ImGui::Text("Parent: %d (%s)", joint.parent.value(), GetJointName(skeleton, joint.parent.value()));
	} else {
		ImGui::TextUnformatted("Parent: none");
	}
	DrawMatrix4x4("SkeletonSpaceMatrix", joint.skeletonSpaceMatrix);
}

void SkinningDebugWindow::DrawVertexInfluence(const Skeleton* skeleton, const Model* model)
{
	if (!ImGui::CollapsingHeader("Vertex Influence", ImGuiTreeNodeFlags_DefaultOpen)) {
		return;
	}

	const uint32_t influenceCount = GetInfluenceCount(model);
	if (!model || !model->HasSkinCluster() || influenceCount == 0) {
		ImGui::TextDisabled("Influence buffer is not available.");
		return;
	}

	ImGui::InputInt("Vertex Index", &selectedVertexIndex_);
	ClampSelections(skeleton, model);

	const Model::SkinCluster& skinCluster = model->GetSkinCluster();
	const Model::VertexInfluence& influence = skinCluster.mappedInfluence[selectedVertexIndex_];
	float weightSum = 0.0f;
	for (uint32_t i = 0; i < Model::kMaxBoneInfluence; ++i) {
		weightSum += influence.weights[i];
	}

	const bool hasInfluence = std::fabs(weightSum) > kWeightEpsilon;
	const bool invalidSum = hasInfluence && std::fabs(weightSum - 1.0f) > kWeightSumTolerance;
	ImGui::TextColored(invalidSum ? ImVec4(1.0f, 0.25f, 0.25f, 1.0f) : ImVec4(0.3f, 0.9f, 0.4f, 1.0f),
		"Weight Sum: %.6f", weightSum);

	if (ImGui::BeginTable("InfluenceTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
		ImGui::TableSetupColumn("Slot");
		ImGui::TableSetupColumn("Weight");
		ImGui::TableSetupColumn("JointIndex");
		ImGui::TableSetupColumn("JointName");
		ImGui::TableHeadersRow();

		for (uint32_t i = 0; i < Model::kMaxBoneInfluence; ++i) {
			const int32_t jointIndex = influence.jointIndices[i];
			const bool active = IsActiveInfluence(influence.weights[i]);
			const bool invalidJoint = active && (!skeleton || jointIndex < 0 || jointIndex >= static_cast<int32_t>(skeleton->joints.size()));

			if (showOnlyErrors_ && !invalidJoint && !invalidSum) {
				continue;
			}

			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::Text("%u", i);
			ImGui::TableSetColumnIndex(1);
			ImGui::Text("%.6f", influence.weights[i]);
			ImGui::TableSetColumnIndex(2);
			ImGui::TextColored(invalidJoint ? ImVec4(1.0f, 0.25f, 0.25f, 1.0f) : ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "%d", jointIndex);
			ImGui::TableSetColumnIndex(3);
			ImGui::TextUnformatted(GetJointName(skeleton, jointIndex));
		}
		ImGui::EndTable();
	}
}

void SkinningDebugWindow::DrawMatrixPalette(Object3d* targetObject, const Skeleton* skeleton, const Model* model)
{
	(void)targetObject;
	if (!ImGui::CollapsingHeader("Matrix Palette", ImGuiTreeNodeFlags_DefaultOpen)) {
		return;
	}

	const uint32_t paletteCount = GetPaletteCount(model);
	if (!model || !model->HasSkinCluster() || !model->GetSkinCluster().mappedPalette || paletteCount == 0) {
		ImGui::TextDisabled("Matrix palette is not available.");
		return;
	}

	ImGui::InputInt("Palette Index", &selectedPaletteIndex_);
	ClampSelections(skeleton, model);

	const Model::SkinCluster& skinCluster = model->GetSkinCluster();
	const std::string& boneName = skinCluster.jointNames[selectedPaletteIndex_];
	ImGui::Text("Palette Count: %u", paletteCount);
	ImGui::Text("Bone Name    : %s", boneName.c_str());

	if (skeleton) {
		auto jointIt = skeleton->jointMap.find(boneName);
		if (jointIt != skeleton->jointMap.end()) {
			ImGui::Text("Joint Index  : %d", jointIt->second);
		} else {
			ImGui::TextColored(ImVec4(1.0f, 0.25f, 0.25f, 1.0f), "Joint Index  : not found");
		}
	}

	const Model::MatrixPalette* mappedPalette = skinCluster.mappedPalette;
	DrawMatrix4x4("SkeletonSpaceMatrix", mappedPalette[selectedPaletteIndex_].skeletonSpaceMatrix);
	DrawMatrix4x4("SkeletonSpaceInverseTransposeMatrix", mappedPalette[selectedPaletteIndex_].skeletonSpaceInverseTransposeMatrix);
}

SkinningDebugWindow::ValidationResult SkinningDebugWindow::Validate(const Skeleton* skeleton, const Model* model) const
{
	ValidationResult result{};
	if (!model || !model->HasSkinCluster()) {
		return result;
	}

	const Model::SkinCluster& skinCluster = model->GetSkinCluster();
	const uint32_t influenceCount = GetInfluenceCount(model);
	const uint32_t jointCount = skeleton ? static_cast<uint32_t>(skeleton->joints.size()) : 0u;
	result.checkedVertexCount = influenceCount;
	result.paletteCountMismatch = skeleton && GetPaletteCount(model) != jointCount;

	if (!skinCluster.mappedInfluence) {
		return result;
	}

	for (uint32_t vertexIndex = 0; vertexIndex < influenceCount; ++vertexIndex) {
		const Model::VertexInfluence& influence = skinCluster.mappedInfluence[vertexIndex];
		float weightSum = 0.0f;
		bool hasInfluence = false;
		bool invalidJoint = false;

		for (uint32_t i = 0; i < Model::kMaxBoneInfluence; ++i) {
			const float weight = influence.weights[i];
			weightSum += weight;
			if (IsActiveInfluence(weight)) {
				hasInfluence = true;
				const int32_t jointIndex = influence.jointIndices[i];
				if (!skeleton || jointIndex < 0 || jointIndex >= static_cast<int32_t>(jointCount)) {
					invalidJoint = true;
				}
			}
		}

		if (!hasInfluence) {
			++result.noInfluenceVertexCount;
		} else if (std::fabs(weightSum - 1.0f) > kWeightSumTolerance) {
			++result.invalidWeightSumCount;
		}

		if (invalidJoint) {
			++result.invalidJointIndexCount;
		}
	}

	return result;
}

void SkinningDebugWindow::ClampSelections(const Skeleton* skeleton, const Model* model)
{
	if (skeleton && !skeleton->joints.empty()) {
		if (selectedJointIndex_ < 0 || selectedJointIndex_ >= static_cast<int32_t>(skeleton->joints.size())) {
			selectedJointIndex_ = skeleton->root >= 0 && skeleton->root < static_cast<int32_t>(skeleton->joints.size()) ? skeleton->root : 0;
		}
	} else {
		selectedJointIndex_ = -1;
	}

	const uint32_t influenceCount = GetInfluenceCount(model);
	if (influenceCount > 0) {
		selectedVertexIndex_ = std::clamp(selectedVertexIndex_, 0, static_cast<int32_t>(influenceCount - 1));
	} else {
		selectedVertexIndex_ = 0;
	}

	const uint32_t paletteCount = GetPaletteCount(model);
	if (paletteCount > 0) {
		selectedPaletteIndex_ = std::clamp(selectedPaletteIndex_, 0, static_cast<int32_t>(paletteCount - 1));
	} else {
		selectedPaletteIndex_ = 0;
	}
}

void SkinningDebugWindow::DrawMatrix4x4(const char* label, const Matrix4x4& matrix) const
{
	if (!ImGui::TreeNode(label)) {
		return;
	}

	for (uint32_t row = 0; row < 4; ++row) {
		ImGui::Text("% .4f  % .4f  % .4f  % .4f",
			matrix.m[row][0], matrix.m[row][1], matrix.m[row][2], matrix.m[row][3]);
	}
	ImGui::TreePop();
}

uint32_t SkinningDebugWindow::GetInfluenceCount(const Model* model) const
{
	if (!model || !model->HasSkinCluster()) {
		return 0;
	}

	const Model::SkinCluster& skinCluster = model->GetSkinCluster();
	if (!skinCluster.mappedInfluence || skinCluster.influenceBufferView.StrideInBytes == 0) {
		return 0;
	}

	return skinCluster.influenceBufferView.SizeInBytes / skinCluster.influenceBufferView.StrideInBytes;
}

uint32_t SkinningDebugWindow::GetPaletteCount(const Model* model) const
{
	if (!model || !model->HasSkinCluster()) {
		return 0;
	}

	return static_cast<uint32_t>(model->GetSkinCluster().jointNames.size());
}
