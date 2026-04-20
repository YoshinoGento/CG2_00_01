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

	// 既存の球体生成
	static std::unique_ptr<Model> CreateSphere(ModelManager* manager, float radius, uint32_t subdivisions);

	// ★追加：平面（Plane）の生成。スライド6「Plane」に対応
	static std::unique_ptr<Model> CreatePlane(ModelManager* manager, float width, float height);

	// ★追加：円（Circle）の生成。スライド5「Circle」に対応
	static std::unique_ptr<Model> CreateCircle(ModelManager* manager, float radius, uint32_t segments);

};
