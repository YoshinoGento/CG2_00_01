#include "3d/JointAttachmentSystem.h"

#include "3d/Object3d.h"
#include "3d/Skeleton.h"
#include "math/Matrix.h"

#include <cmath>

namespace {
constexpr float kMinimumSocketScaleMagnitude = 1.0e-5f;

bool IsFiniteVector(const Vector3& value) {
	return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

bool HasUsableScale(const Vector3& scale) {
	return std::abs(scale.x) >= kMinimumSocketScaleMagnitude &&
		std::abs(scale.y) >= kMinimumSocketScaleMagnitude &&
		std::abs(scale.z) >= kMinimumSocketScaleMagnitude;
}
} // namespace

bool JointAttachmentSystem::Bind(
	Object3d* child,
	const Object3d* source,
	const BindingSettings& settings) {
	if (!child || !BindSocket(source, settings)) {
		return false;
	}

	child_ = child;
	if (!SetLocalTransform(localTransform_)) {
		Clear();
		return false;
	}
	return CanResolveJoint();
}

bool JointAttachmentSystem::BindSocket(
	const Object3d* source,
	const BindingSettings& settings) {
	Clear();
	if (!source || settings.jointName.empty()) {
		return false;
	}

	source_ = source;
	jointName_ = settings.jointName;
	if (!SetLocalTransform(settings.localTransform)) {
		Clear();
		return false;
	}
	return CanResolveJoint();
}

void JointAttachmentSystem::Clear() {
	if (child_) {
		child_->ClearParentWorldMatrix();
	}
	child_ = nullptr;
	source_ = nullptr;
	jointName_.clear();
	localTransform_ = {
		{ 1.0f, 1.0f, 1.0f },
		{ 0.0f, 0.0f, 0.0f },
		{ 0.0f, 0.0f, 0.0f },
	};
	resolvedParentScale_ = { 1.0f, 1.0f, 1.0f };
	resolvedWorldMatrix_ = {};
	resolvedWorldPosition_ = { 0.0f, 0.0f, 0.0f };
	resolved_ = false;
}

void JointAttachmentSystem::SetEnabled(bool enabled) {
	enabled_ = enabled;
	if (!enabled_) {
		if (child_) {
			child_->ClearParentWorldMatrix();
		}
		resolvedParentScale_ = { 1.0f, 1.0f, 1.0f };
		resolvedWorldMatrix_ = {};
		resolvedWorldPosition_ = { 0.0f, 0.0f, 0.0f };
		resolved_ = false;
	}
}

bool JointAttachmentSystem::SetLocalTransform(const Transform& localTransform) {
	if (!IsFiniteVector(localTransform.scale) ||
		!IsFiniteVector(localTransform.rotate) ||
		!IsFiniteVector(localTransform.translate) ||
		!HasUsableScale(localTransform.scale)) {
		return false;
	}

	if (child_) {
		if (!child_->SetScale(localTransform.scale)) {
			return false;
		}
		child_->SetRotation(localTransform.rotate);
		child_->SetPosition(localTransform.translate);
	}
	localTransform_ = localTransform;
	return true;
}

bool JointAttachmentSystem::CanResolveJoint() const {
	if (!IsBound() || jointName_.empty()) {
		return false;
	}
	const std::optional<Skeleton>& skeleton = source_->GetSkeleton();
	if (!skeleton.has_value()) {
		return false;
	}
	const auto jointIt = skeleton->jointMap.find(jointName_);
	return jointIt != skeleton->jointMap.end() &&
		jointIt->second >= 0 &&
		jointIt->second < static_cast<int32_t>(skeleton->joints.size());
}

bool JointAttachmentSystem::Update() {
	resolved_ = false;
	if (!enabled_ || !CanResolveJoint()) {
		if (child_) {
			child_->ClearParentWorldMatrix();
		}
		resolvedParentScale_ = { 1.0f, 1.0f, 1.0f };
		resolvedWorldMatrix_ = {};
		resolvedWorldPosition_ = { 0.0f, 0.0f, 0.0f };
		return false;
	}

	const Skeleton& skeleton = source_->GetSkeleton().value();
	const int32_t jointIndex = skeleton.jointMap.at(jointName_);
	const Matrix4x4 parentWorldMatrix = MatrixMath::Multiply(
		skeleton.joints[static_cast<size_t>(jointIndex)].skeletonSpaceMatrix,
		source_->GetObjectWorldMatrix());
	if (!HasFiniteParentMatrix(parentWorldMatrix)) {
		if (child_) {
			child_->ClearParentWorldMatrix();
		}
		resolvedParentScale_ = { 1.0f, 1.0f, 1.0f };
		resolvedWorldMatrix_ = {};
		resolvedWorldPosition_ = { 0.0f, 0.0f, 0.0f };
		return false;
	}

	resolvedParentScale_ = {
		std::sqrt(
			parentWorldMatrix.m[0][0] * parentWorldMatrix.m[0][0] +
			parentWorldMatrix.m[0][1] * parentWorldMatrix.m[0][1] +
			parentWorldMatrix.m[0][2] * parentWorldMatrix.m[0][2]),
		std::sqrt(
			parentWorldMatrix.m[1][0] * parentWorldMatrix.m[1][0] +
			parentWorldMatrix.m[1][1] * parentWorldMatrix.m[1][1] +
			parentWorldMatrix.m[1][2] * parentWorldMatrix.m[1][2]),
		std::sqrt(
			parentWorldMatrix.m[2][0] * parentWorldMatrix.m[2][0] +
			parentWorldMatrix.m[2][1] * parentWorldMatrix.m[2][1] +
			parentWorldMatrix.m[2][2] * parentWorldMatrix.m[2][2]),
	};
	if (!IsFiniteVector(resolvedParentScale_) || !HasUsableScale(resolvedParentScale_)) {
		if (child_) {
			child_->ClearParentWorldMatrix();
		}
		resolvedParentScale_ = { 1.0f, 1.0f, 1.0f };
		resolvedWorldMatrix_ = {};
		resolvedWorldPosition_ = { 0.0f, 0.0f, 0.0f };
		return false;
	}
	resolvedWorldMatrix_ = MatrixMath::Multiply(
		MatrixMath::MakeAffineMatrix(localTransform_.scale, localTransform_.rotate, localTransform_.translate),
		parentWorldMatrix);
	if (!HasFiniteParentMatrix(resolvedWorldMatrix_)) {
		if (child_) {
			child_->ClearParentWorldMatrix();
		}
		resolvedParentScale_ = { 1.0f, 1.0f, 1.0f };
		resolvedWorldMatrix_ = {};
		resolvedWorldPosition_ = { 0.0f, 0.0f, 0.0f };
		return false;
	}
	resolvedWorldPosition_ = MatrixMath::Transform({ 0.0f, 0.0f, 0.0f }, resolvedWorldMatrix_);
	if (!IsFiniteVector(resolvedWorldPosition_)) {
		if (child_) {
			child_->ClearParentWorldMatrix();
		}
		resolvedParentScale_ = { 1.0f, 1.0f, 1.0f };
		resolvedWorldMatrix_ = {};
		resolvedWorldPosition_ = { 0.0f, 0.0f, 0.0f };
		return false;
	}
	if (child_) {
		child_->SetParentWorldMatrix(parentWorldMatrix);
	}
	resolved_ = true;
	return true;
}

bool JointAttachmentSystem::HasFiniteParentMatrix(const Matrix4x4& matrix) const {
	for (const auto& row : matrix.m) {
		for (float value : row) {
			if (!std::isfinite(value)) {
				return false;
			}
		}
	}
	return true;
}
