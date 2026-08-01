#include "3d/SkeletonDebugger.h"
#include "3d/PrimitiveGenerator.h"
#include "math/Matrix.h"

#include <algorithm>
#include <cmath>

namespace {
float LengthSquared(const Vector3& value) {
	return value.x * value.x + value.y * value.y + value.z * value.z;
}

Vector3 NormalizeVector(const Vector3& value) {
	const float lengthSquared = LengthSquared(value);
	if (!std::isfinite(lengthSquared) || lengthSquared <= 1.0e-8f) {
		return {};
	}
	const float inverseLength = 1.0f / std::sqrt(lengthSquared);
	return { value.x * inverseLength, value.y * inverseLength, value.z * inverseLength };
}

Vector3 NormalizeAxis(const Matrix4x4& matrix, size_t row) {
	return NormalizeVector({ matrix.m[row][0], matrix.m[row][1], matrix.m[row][2] });
}

Vector3 AddScaled(const Vector3& origin, const Vector3& direction, float scale) {
	return {
		origin.x + direction.x * scale,
		origin.y + direction.y * scale,
		origin.z + direction.z * scale,
	};
}

Vector3 Subtract(const Vector3& lhs, const Vector3& rhs) {
	return { lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z };
}

Vector3 Cross(const Vector3& lhs, const Vector3& rhs) {
	return {
		lhs.y * rhs.z - lhs.z * rhs.y,
		lhs.z * rhs.x - lhs.x * rhs.z,
		lhs.x * rhs.y - lhs.y * rhs.x,
	};
}

void DrawSelectedBoneOutline(
	const Vector3& parentPosition,
	const Vector3& selectedPosition,
	const Vector3& cameraPosition,
	LineDrawer* lineDrawer) {
	const Vector3 boneVector = Subtract(selectedPosition, parentPosition);
	const float boneLengthSquared = LengthSquared(boneVector);
	if (!lineDrawer || !std::isfinite(boneLengthSquared) || boneLengthSquared <= 1.0e-8f) {
		return;
	}

	const float boneLength = std::sqrt(boneLengthSquared);
	const Vector3 boneDirection = NormalizeVector(boneVector);
	const Vector3 midpoint = AddScaled(parentPosition, boneDirection, boneLength * 0.5f);
	Vector3 side = NormalizeVector(Cross(boneDirection, Subtract(cameraPosition, midpoint)));
	if (LengthSquared(side) <= 1.0e-8f) {
		side = NormalizeVector(Cross(boneDirection, { 0.0f, 1.0f, 0.0f }));
	}
	if (LengthSquared(side) <= 1.0e-8f) {
		side = NormalizeVector(Cross(boneDirection, { 1.0f, 0.0f, 0.0f }));
	}
	const Vector3 up = NormalizeVector(Cross(side, boneDirection));
	if (LengthSquared(side) <= 1.0e-8f || LengthSquared(up) <= 1.0e-8f) {
		return;
	}

	const float radius = std::clamp(boneLength * 0.16f, 0.04f, 0.10f);
	const Vector3 ring[4] = {
		AddScaled(midpoint, side, radius),
		AddScaled(midpoint, up, radius),
		AddScaled(midpoint, side, -radius),
		AddScaled(midpoint, up, -radius),
	};
	constexpr Vector4 kSelectedBoneColor = { 1.0f, 0.92f, 0.05f, 1.0f };
	for (size_t index = 0; index < 4; ++index) {
		const size_t next = (index + 1) % 4;
		lineDrawer->DrawLine(parentPosition, ring[index], kSelectedBoneColor);
		lineDrawer->DrawLine(ring[index], selectedPosition, kSelectedBoneColor);
		lineDrawer->DrawLine(ring[index], ring[next], kSelectedBoneColor);
	}
}

bool IsValidJointIndex(const Skeleton& skeleton, int32_t jointIndex) {
	return jointIndex >= 0 && jointIndex < static_cast<int32_t>(skeleton.joints.size());
}

bool IsJointOnSelectedChain(const Skeleton& skeleton, int32_t candidateIndex, int32_t selectedIndex) {
	if (!IsValidJointIndex(skeleton, candidateIndex) || !IsValidJointIndex(skeleton, selectedIndex)) {
		return false;
	}

	int32_t currentIndex = selectedIndex;
	for (size_t depth = 0; depth < skeleton.joints.size(); ++depth) {
		if (currentIndex == candidateIndex) {
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
} // namespace

void SkeletonDebugger::Initialize(Object3dCommon* object3dCommon, ModelManager* modelManager) {
	// Object3d の代わりにワイヤーフレームを使うため、ここでは特に何もしません
	(void)object3dCommon;
	(void)modelManager;
}

void SkeletonDebugger::Draw(const Skeleton& skeleton, const Matrix4x4& worldMatrix, LineDrawer* lineDrawer, Camera* camera) {
	if (!lineDrawer || !camera) return;

	// 選択中のジョイントインデックス（範囲外の場合は0）
	int32_t activeJoint = selectedJointIndex_;
	if (activeJoint < 0 || activeJoint >= static_cast<int32_t>(skeleton.joints.size())) {
		activeJoint = -1;
	}

	// 各Jointの座標計算と描画処理
	for (size_t i = 0; i < skeleton.joints.size(); ++i) {
		const Joint& joint = skeleton.joints[i];
		// Jointのスケルトン空間行列に、オブジェクト自体の純粋なトランスフォーム行列を掛け合わせる
		Matrix4x4 jointWorldMatrix = MatrixMath::Multiply(joint.skeletonSpaceMatrix, worldMatrix);
		
		// 1. 関節（ワイヤーフレーム球）の描画
		// 平行移動（Translate）要素を抽出して関節座標とする
		Vector3 position = { jointWorldMatrix.m[3][0], jointWorldMatrix.m[3][1], jointWorldMatrix.m[3][2] };
		
		const bool isSelected = i == static_cast<size_t>(activeJoint);
		const bool isRoot = joint.index == skeleton.root;
		const bool isOnSelectedChain = highlightSelectedChain_ &&
			IsJointOnSelectedChain(skeleton, joint.index, activeJoint);
		const Vector4 color = isSelected
			? Vector4{ 1.0f, 0.92f, 0.05f, 1.0f }
			: isOnSelectedChain
				? Vector4{ 0.78f, 0.18f, 0.58f, 1.0f }
				: isRoot
					? Vector4{ 0.25f, 1.0f, 0.35f, 1.0f }
					: Vector4{ 0.25f, 0.85f, 1.0f, 1.0f };
		if (showJointMarkers_) {
			lineDrawer->DrawWireSphere(position, isSelected ? 0.09f : 0.035f, color, isSelected ? 12u : 6u);
		}

		if (showAllLocalAxes_ || (isSelected && showLocalAxes_)) {
			const float axisLength = isSelected ? 0.35f : 0.12f;
			const Vector3 xAxis = NormalizeAxis(jointWorldMatrix, 0);
			const Vector3 yAxis = NormalizeAxis(jointWorldMatrix, 1);
			const Vector3 zAxis = NormalizeAxis(jointWorldMatrix, 2);
			lineDrawer->DrawLine(position, AddScaled(position, xAxis, axisLength), { 1.0f, 0.15f, 0.15f, 1.0f });
			lineDrawer->DrawLine(position, AddScaled(position, yAxis, axisLength), { 0.15f, 1.0f, 0.20f, 1.0f });
			lineDrawer->DrawLine(position, AddScaled(position, zAxis, axisLength), { 0.20f, 0.45f, 1.0f, 1.0f });
		}

		// 2. 骨（線）の描画
		// 親が存在する場合、親の関節位置から自分の関節位置に向けて線を引く
		if (joint.parent && IsValidJointIndex(skeleton, *joint.parent)) {
			Matrix4x4 parentWorldMatrix = MatrixMath::Multiply(
				skeleton.joints[static_cast<size_t>(*joint.parent)].skeletonSpaceMatrix, worldMatrix);
			Vector3 parentPosition = { parentWorldMatrix.m[3][0], parentWorldMatrix.m[3][1], parentWorldMatrix.m[3][2] };
			const Vector4 boneColor = isSelected
				? Vector4{ 1.0f, 0.92f, 0.05f, 1.0f }
				: isOnSelectedChain
					? Vector4{ 0.78f, 0.18f, 0.58f, 1.0f }
					: Vector4{ 0.38f, 0.43f, 0.52f, 1.0f };
			lineDrawer->DrawLine(parentPosition, position, boneColor);
			if (isSelected) {
				DrawSelectedBoneOutline(parentPosition, position, camera->GetTranslate(), lineDrawer);
				lineDrawer->DrawWireSphere(parentPosition, 0.055f, { 1.0f, 0.55f, 0.05f, 1.0f }, 10u);
			}
		}
	}
}
