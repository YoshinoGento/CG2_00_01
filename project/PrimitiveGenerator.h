#pragma once
#include "Model.h"
#include <vector>

/**
 * PrimitiveGenerator
 * 球体や立方体などの基本形状をプログラムで生成するクラス
 */
class PrimitiveGenerator {
public:
    // 立方体（ブロック）のモデルを作成
    static std::unique_ptr<Model> CreateBox(ModelManager* manager, const Vector3& size = { 1.0f, 1.0f, 1.0f });

    // 球体のモデルを作成
    // subdivisions: 分割数（多いほど滑らかになる）
    static std::unique_ptr<Model> CreateSphere(ModelManager* manager, float radius = 1.0f, uint32_t subdivisions = 16);
};