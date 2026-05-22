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

	// 環境マップのハンドルをセット（保持している全ての球オブジェクトに反映）
	void SetEnvironmentMap(uint32_t handle) {
		environmentMapHandle_ = handle;
		for (auto& sphere : jointSpheres_) {
			if (sphere) {
				sphere->SetEnvironmentMap(handle);
			}
		}
	}

	void DrawImGui(Skeleton& skeleton);

private:
	Object3dCommon* object3dCommon_ = nullptr;
	std::unique_ptr<Model> jointModel_;
	
	// 各ジョイントごとに固有の定数バッファを割り当てるため、個別のObject3dインスタンスを管理します
	std::vector<std::unique_ptr<Object3d>> jointSpheres_;

	// 環境マップのハンドルを保持（リサイズ時に割り当てるため）
	uint32_t environmentMapHandle_ = 0;

	int32_t selectedJointIndex_ = 0;
};

