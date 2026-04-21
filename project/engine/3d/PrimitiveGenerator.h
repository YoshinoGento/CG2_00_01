#pragma once
#include "3d/Model.h"
#include <memory>
#include <vector>

#pragma once
#include "Model.h"
#include <memory>
#include <vector>

/**
 * PrimitiveGenerator
 * 球体や立方体などの基本形状をプログラムで生成するクラス
 */
class PrimitiveGenerator {
public:
	// 立方体（ブロック）のモデルを作成
	static std::unique_ptr<Model> CreateBox(ModelManager* manager, const Vector3& size = { 1.0f, 1.0f, 1.0f });

	// 球体生成
	static std::unique_ptr<Model> CreateSphere(ModelManager* manager, float radius, uint32_t subdivisions);

	// 平面（Plane）の生成（スライド6対応）
	static std::unique_ptr<Model> CreatePlane(ModelManager* manager, float width, float height);

	// 円（Circle）の生成（スライド5対応）
	static std::unique_ptr<Model> CreateCircle(ModelManager* manager, float radius, uint32_t segments);

	// 円輪（Ring）の生成（スライド5対応）
	static std::unique_ptr<Model> CreateRing(ModelManager* manager, float innerRadius, float outerRadius, uint32_t segments);

	// 円柱（Cylinder）の生成（スライド5対応）
	static std::unique_ptr<Model> CreateCylinder(ModelManager* manager, float radius, float height, uint32_t segments);
};
