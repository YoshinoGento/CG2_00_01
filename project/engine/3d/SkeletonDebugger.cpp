#include "3d/SkeletonDebugger.h"
#include "3d/PrimitiveGenerator.h"
#include "math/Matrix.h"

void SkeletonDebugger::Initialize(Object3dCommon* object3dCommon, ModelManager* modelManager) {
	object3dCommon_ = object3dCommon;

	// 関節表示用の球モデルを生成（白く描画するために小さめの球を準備）
	jointModel_ = PrimitiveGenerator::CreateSphere(modelManager, 0.05f, 8);
}

void SkeletonDebugger::Draw(const Skeleton& skeleton, const Matrix4x4& worldMatrix, LineDrawer* lineDrawer, Camera* camera) {
	if (!lineDrawer || !object3dCommon_ || !camera) return;

	// 関節の数に合わせてObject3dの動的配列をリサイズし、新しく追加された分を初期化する
	if (jointSpheres_.size() < skeleton.joints.size()) {
		size_t prevSize = jointSpheres_.size();
		jointSpheres_.resize(skeleton.joints.size());
		for (size_t i = prevSize; i < skeleton.joints.size(); ++i) {
			jointSpheres_[i] = std::make_unique<Object3d>();
			jointSpheres_[i]->Initialize(object3dCommon_);
			jointSpheres_[i]->SetModel(jointModel_.get());
			
			// 白く自己発光（フラットな白色）に設定
			jointSpheres_[i]->GetMaterialData()->enableLighting = 0;
			jointSpheres_[i]->GetMaterialData()->color = { 1.0f, 1.0f, 1.0f, 1.0f };
			
			// 環境マップが事前に設定されていれば適用する
			if (environmentMapHandle_ != 0) {
				jointSpheres_[i]->SetEnvironmentMap(environmentMapHandle_);
			}
		}
	}

	// 各Jointの座標計算と描画処理
	for (size_t i = 0; i < skeleton.joints.size(); ++i) {
		const Joint& joint = skeleton.joints[i];
		// Jointのスケルトン空間行列に、オブジェクト自体の純粋なトランスフォーム行列を掛け合わせる
		Matrix4x4 jointWorldMatrix = MatrixMath::Multiply(joint.skeletonSpaceMatrix, worldMatrix);
		
		// 1. 関節（球）の描画
		// 平行移動（Translate）要素を抽出して関節座標とする
		Vector3 position = { jointWorldMatrix.m[3][0], jointWorldMatrix.m[3][1], jointWorldMatrix.m[3][2] };
		
		jointSpheres_[i]->SetPosition(position);
		// Updateで内部の個別の定数バッファ(WVP)を再計算
		jointSpheres_[i]->Update(camera);
		
		// 個別のObject3dを用いて描画（定数バッファの上書き衝突を回避）
		jointSpheres_[i]->Draw();

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
