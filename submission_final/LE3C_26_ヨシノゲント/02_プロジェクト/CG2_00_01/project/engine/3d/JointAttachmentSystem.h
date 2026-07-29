#pragma once

#include "math/Transform.h"

#include <string>

class Object3d;

// Resolves a Skeleton Joint into a parent matrix for one Scene-owned child.
// Object lifetimes remain owned by the Scene and must outlive this binding.
class JointAttachmentSystem final {
public:
	struct BindingSettings {
		std::string jointName;
		Transform localTransform{
			{ 1.0f, 1.0f, 1.0f },
			{ 0.0f, 0.0f, 0.0f },
			{ 0.0f, 0.0f, 0.0f },
		};
	};

	[[nodiscard]] bool Bind(Object3d* child, const Object3d* source, const BindingSettings& settings);
	[[nodiscard]] bool BindSocket(const Object3d* source, const BindingSettings& settings);
	void Clear();
	void SetEnabled(bool enabled);
	[[nodiscard]] bool SetLocalTransform(const Transform& localTransform);
	[[nodiscard]] bool Update();

	[[nodiscard]] bool IsBound() const { return source_ != nullptr; }
	[[nodiscard]] bool HasChild() const { return child_ != nullptr; }
	[[nodiscard]] bool CanResolveJoint() const;
	[[nodiscard]] bool IsResolved() const { return resolved_; }
	[[nodiscard]] bool IsEnabled() const { return enabled_; }
	[[nodiscard]] const std::string& GetJointName() const { return jointName_; }
	[[nodiscard]] const Transform& GetLocalTransform() const { return localTransform_; }
	[[nodiscard]] const Vector3& GetResolvedParentScale() const { return resolvedParentScale_; }
	[[nodiscard]] const Matrix4x4& GetResolvedWorldMatrix() const { return resolvedWorldMatrix_; }
	[[nodiscard]] const Vector3& GetResolvedWorldPosition() const { return resolvedWorldPosition_; }

private:
	[[nodiscard]] bool HasFiniteParentMatrix(const Matrix4x4& matrix) const;

	Object3d* child_ = nullptr;
	const Object3d* source_ = nullptr;
	std::string jointName_;
	Transform localTransform_ = {
		{ 1.0f, 1.0f, 1.0f },
		{ 0.0f, 0.0f, 0.0f },
		{ 0.0f, 0.0f, 0.0f },
	};
	Vector3 resolvedParentScale_ = { 1.0f, 1.0f, 1.0f };
	Matrix4x4 resolvedWorldMatrix_{};
	Vector3 resolvedWorldPosition_ = { 0.0f, 0.0f, 0.0f };
	bool enabled_ = true;
	bool resolved_ = false;
};
