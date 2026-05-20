#include "3d/SkeletonDebugger.h"
#include "3d/PrimitiveGenerator.h"
#include "math/Matrix.h"

void SkeletonDebugger::Initialize(Object3dCommon* object3dCommon, ModelManager* modelManager) {
	object3dCommon_ = object3dCommon;

	// 関節表示用の球モデルを生成
	jointModel_ = PrimitiveGenerator::CreateSphere(modelManager, 0.05f, 8); // 小さめの球

	// 球描画用のオブジェクト
	jointSphere_ = std::make_unique<Object3d>();
	jointSphere_->Initialize(object3dCommon);
	jointSphere_->SetModel(jointModel_.get());

	// ライトの影響を受けないようにフラットな色にする（必要に応じて）
	jointSphere_->SetEnableLighting(0);
}

void SkeletonDebugger::Draw(const Skeleton& skeleton, const Matrix4x4& worldMatrix, LineDrawer* lineDrawer, Camera* camera) {
	if (!lineDrawer || !object3dCommon_ || !camera) return;

	// 【解説】
	// 各Jointについて、 skeletonSpaceMatrix * オブジェクト自身のworldMatrix を計算し、
	// 最終的なワールド座標（jointWorldMatrix）を求めます。
	
	for (const Joint& joint : skeleton.joints) {
		Matrix4x4 jointWorldMatrix = MatrixMath::Multiply(joint.skeletonSpaceMatrix, worldMatrix);
		
		// 1. 関節（球）の描画
		// Translate要素だけを抽出してSphereを配置します
		Vector3 position = { jointWorldMatrix.m[3][0], jointWorldMatrix.m[3][1], jointWorldMatrix.m[3][2] };
		
		jointSphere_->SetPosition(position);
		// Updateで内部のworldMatrixを再計算
		jointSphere_->Update(camera);
		
		// 描画
		// ※注意: Object3d::Draw()は内部でCameraのビュープロジェクション等が必要ですが、
		// ここでは簡略化のため、事前設定されたカメラ情報を使って描画する設計になっていると仮定します。
		// 本来はDrawにCameraを渡すか、Object3dCommonにセットされた情報を利用します。
		jointSphere_->Draw();

		// 2. 骨（線）の描画
		// 親がいる場合、親の位置から自分の位置へ線を引く
		if (joint.parent) {
			Matrix4x4 parentWorldMatrix = MatrixMath::Multiply(skeleton.joints[*joint.parent].skeletonSpaceMatrix, worldMatrix);
			Vector3 parentPosition = { parentWorldMatrix.m[3][0], parentWorldMatrix.m[3][1], parentWorldMatrix.m[3][2] };
			
			// 白い線で親から子へ引く
			lineDrawer->DrawLine(parentPosition, position, {1.0f, 1.0f, 1.0f, 1.0f});
		}
	}
}
