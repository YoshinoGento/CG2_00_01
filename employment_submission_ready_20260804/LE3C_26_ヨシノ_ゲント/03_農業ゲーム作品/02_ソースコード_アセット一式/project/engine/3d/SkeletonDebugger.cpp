#include "3d/SkeletonDebugger.h"
#include "3d/PrimitiveGenerator.h"
#include "math/Matrix.h"

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
		
		// 選択中のジョイントは赤色、それ以外は白色で描画
		Vector4 color = (i == static_cast<size_t>(activeJoint)) ? Vector4{1.0f, 0.0f, 0.0f, 1.0f} : Vector4{1.0f, 1.0f, 1.0f, 1.0f};
		
		// 球体の半径は 0.05f 程度
		lineDrawer->DrawWireSphere(position, 0.05f, color, 8);

		// 2. 骨（線）の描画
		// 親が存在する場合、親の関節位置から自分の関節位置に向けて線を引く
		if (joint.parent) {
			Matrix4x4 parentWorldMatrix = MatrixMath::Multiply(skeleton.joints[*joint.parent].skeletonSpaceMatrix, worldMatrix);
			Vector3 parentPosition = { parentWorldMatrix.m[3][0], parentWorldMatrix.m[3][1], parentWorldMatrix.m[3][2] };
			
			// 白い線で親から子へ引く
			lineDrawer->DrawLine(parentPosition, position, {1.0f, 1.0f, 1.0f, 1.0f});
		}
	}
}
