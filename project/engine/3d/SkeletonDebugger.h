#pragma once
#include "3d/Skeleton.h"
#include "3d/Object3d.h"
#include "3d/Model.h"
#include "3d/LineDrawer.h"
#include <memory>
#include <vector>

class Object3dCommon;
class ModelManager;

/**
 * SkeletonDebugger
 * スケルトンの状態（関節と骨）を可視化するためのクラス
 * 授業資料の「デバッグ描画」に相当します。
 */
class SkeletonDebugger {
public:
	// 初期化：関節表示用の球モデルなどを準備
	void Initialize(Object3dCommon* object3dCommon, ModelManager* modelManager);

	// 描画：スケルトンの各関節に球を配置し、親子間に線を引く
	void Draw(const Skeleton& skeleton, const Matrix4x4& worldMatrix, LineDrawer* lineDrawer, Camera* camera);

private:
	Object3dCommon* object3dCommon_ = nullptr;
	std::unique_ptr<Model> jointModel_;
	
	// パフォーマンスと手間の観点から、各JointごとにObject3dを持つより、
	// 描画時に1つのObject3dを使い回す設計にします
	std::unique_ptr<Object3d> jointSphere_;
};
