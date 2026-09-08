#pragma once
#include "3d/Model.h"
#include <memory>
#include <vector>
#include <numbers>

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

    // 平面（Plane）の生成
    static std::unique_ptr<Model> CreatePlane(ModelManager* manager, float width, float height);

    // 円（Circle）の生成
    static std::unique_ptr<Model> CreateCircle(ModelManager* manager, float radius, uint32_t segments);

    // 円輪（Ring）の生成（資料8枚目の拡張対応版）
    static std::unique_ptr<Model> CreateRing(
        ModelManager* manager,
        float innerRadius,
        float outerRadius,
        uint32_t segments,
        float startAngle = 0.0f,
        float endAngle = 2.0f * std::numbers::pi_v<float>
    );

    // 円柱（Cylinder）の生成
    // topRadius / bottomRadius: 上面と下面の半径（違う値にすると円錐台になる）
    // height: 円柱の高さ
    // segments: 円周方向の分割数（多いほど滑らか）
    // verticalDivisions: 高さ方向の分割数（頂点カラーグラデーション用、1なら上下2段のみ）
    // startAngle / endAngle: 生成する角度範囲（ラジアン、デフォルトは全周2π）
    static std::unique_ptr<Model> CreateCylinder(
        ModelManager* manager,
        float topRadius,
        float bottomRadius,
        float height,
        uint32_t segments,
        uint32_t verticalDivisions = 1,
        float startAngle = 0.0f,
        float endAngle = 2.0f * std::numbers::pi_v<float>
    );
};