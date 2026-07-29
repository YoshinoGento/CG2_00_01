#include "3d/SkeletonDebugger.h"
#include "3d/PrimitiveGenerator.h"
#include "math/Matrix.h"

#include <cmath>

namespace {
Vector3 NormalizeAxis(const Matrix4x4& matrix, size_t row) {
	const Vector3 axis = { matrix.m[row][0], matrix.m[row][1], matrix.m[row][2] };
	const float lengthSquared = axis.x * axis.x + axis.y * axis.y + axis.z * axis.z;
	if (!std::isfinite(lengthSquared) || lengthSquared <= 1.0e-8f) {
		return {};
	}
	const float inverseLength = 1.0f / std::sqrt(lengthSquared);
	return { axis.x * inverseLength, axis.y * inverseLength, axis.z * inverseLength };
}

Vector3 AddScaled(const Vector3& origin, const Vector3& direction, float scale) {
	return {
		origin.x + direction.x * scale,
		origin.y + direction.y * scale,
		origin.z + direction.z * scale,
	};
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
		const Vector4 color = isSelected
			? Vector4{ 1.0f, 0.75f, 0.10f, 1.0f }
			: Vector4{ 0.25f, 0.85f, 1.0f, 1.0f };
		lineDrawer->DrawWireSphere(position, isSelected ? 0.08f : 0.035f, color, isSelected ? 10u : 6u);

		if (isSelected && showLocalAxes_) {
			constexpr float kAxisLength = 0.35f;
			const Vector3 xAxis = NormalizeAxis(jointWorldMatrix, 0);
			const Vector3 yAxis = NormalizeAxis(jointWorldMatrix, 1);
			const Vector3 zAxis = NormalizeAxis(jointWorldMatrix, 2);
			lineDrawer->DrawLine(position, AddScaled(position, xAxis, kAxisLength), { 1.0f, 0.15f, 0.15f, 1.0f });
			lineDrawer->DrawLine(position, AddScaled(position, yAxis, kAxisLength), { 0.15f, 1.0f, 0.20f, 1.0f });
			lineDrawer->DrawLine(position, AddScaled(position, zAxis, kAxisLength), { 0.20f, 0.45f, 1.0f, 1.0f });
		}

		// 2. 骨（線）の描画
		// 親が存在する場合、親の関節位置から自分の関節位置に向けて線を引く
		if (joint.parent) {
			Matrix4x4 parentWorldMatrix = MatrixMath::Multiply(skeleton.joints[*joint.parent].skeletonSpaceMatrix, worldMatrix);
			Vector3 parentPosition = { parentWorldMatrix.m[3][0], parentWorldMatrix.m[3][1], parentWorldMatrix.m[3][2] };
			
			lineDrawer->DrawLine(parentPosition, position, { 0.70f, 0.75f, 0.82f, 1.0f });
		}
	}
}
