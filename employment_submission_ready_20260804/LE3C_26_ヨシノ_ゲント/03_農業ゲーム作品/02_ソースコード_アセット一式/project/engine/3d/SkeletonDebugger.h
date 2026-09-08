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

	// 環境マップのハンドル（以前の互換性のために残すか空にする）
	void SetEnvironmentMap(TextureCubeHandle handle) {
		// ワイヤーフレーム描画では環境マップを使用しないため何もしない
		(void)handle;
	}

	void DrawImGui(Skeleton& skeleton);

private:
	int32_t selectedJointIndex_ = 0;
};

