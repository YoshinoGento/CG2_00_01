#include "3d/PrimitiveGenerator.h"
#include "math/Matrix.h"
#include <numbers>
#include <cmath>
#include <algorithm>

/**
 * 立方体の生成
 */
std::unique_ptr<Model> PrimitiveGenerator::CreateBox(ModelManager* manager, const Vector3& size) {
    float hx = size.x * 0.5f;
    float hy = size.y * 0.5f;
    float hz = size.z * 0.5f;

    std::vector<Model::VertexData> vertices = {
        // 前 (Z-)
        {{-hx,  hy, -hz, 1.0f}, {0, 0, -1}, {0, 0}}, {{ hx,  hy, -hz, 1.0f}, {0, 0, -1}, {1, 0}},
        {{-hx, -hy, -hz, 1.0f}, {0, 0, -1}, {0, 1}}, {{ hx, -hy, -hz, 1.0f}, {0, 0, -1}, {1, 1}},
        // 後 (Z+)
        {{ hx,  hy,  hz, 1.0f}, {0, 0,  1}, {0, 0}}, {{-hx,  hy,  hz, 1.0f}, {0, 0,  1}, {1, 0}},
        {{ hx, -hy,  hz, 1.0f}, {0, 0,  1}, {0, 1}}, {{-hx, -hy,  hz, 1.0f}, {0, 0,  1}, {1, 1}},
        // 上 (Y+)
        {{-hx,  hy,  hz, 1.0f}, {0, 1,  0}, {0, 0}}, {{ hx,  hy,  hz, 1.0f}, {0, 1,  0}, {1, 0}},
        {{-hx,  hy, -hz, 1.0f}, {0, 1,  0}, {0, 1}}, {{ hx,  hy, -hz, 1.0f}, {0, 1,  0}, {1, 1}},
        // 下 (Y-)
        {{-hx, -hy, -hz, 1.0f}, {0, -1, 0}, {0, 0}}, {{ hx, -hy, -hz, 1.0f}, {0, -1, 0}, {1, 0}},
        {{-hx, -hy,  hz, 1.0f}, {0, -1, 0}, {0, 1}}, {{-hx, -hy,  hz, 1.0f}, {0, -1, 0}, {1, 1}},
        // 右 (X+)
        {{ hx,  hy, -hz, 1.0f}, {1, 0,  0}, {0, 0}}, {{ hx,  hy,  hz, 1.0f}, {1, 0,  0}, {1, 0}},
        {{ hx, -hy, -hz, 1.0f}, {1, 0,  0}, {0, 1}}, {{ hx,  hy,  hz, 1.0f}, {1, 0,  0}, {1, 1}},
        // 左 (X-)
        {{-hx,  hy,  hz, 1.0f}, {-1, 0, 0}, {0, 0}}, {{-hx,  hy, -hz, 1.0f}, {-1, 0, 0}, {1, 0}},
        {{-hx, -hy,  hz, 1.0f}, {-1, 0, 0}, {0, 1}}, {{-hx, -hy, -hz, 1.0f}, {-1, 0, 0}, {1, 1}},
    };

    std::vector<uint32_t> indices;
    for (int i = 0; i < 6; ++i) {
        uint32_t start = i * 4;
        indices.push_back(start); indices.push_back(start + 1); indices.push_back(start + 2);
        indices.push_back(start + 1); indices.push_back(start + 3); indices.push_back(start + 2);
    }

    auto model = std::make_unique<Model>();
    model->InitializeWithData(manager, vertices, indices);
    return model;
}

/**
 * 球体の生成
 */
std::unique_ptr<Model> PrimitiveGenerator::CreateSphere(ModelManager* manager, float radius, uint32_t subdivisions) {
    std::vector<Model::VertexData> vertices;
    std::vector<uint32_t> indices;

    const float pi = std::numbers::pi_v<float>;
    const float lonStep = 2.0f * pi / subdivisions;
    const float latStep = pi / subdivisions;

    for (uint32_t lat = 0; lat <= subdivisions; ++lat) {
        float phi = pi / 2.0f - (float)lat * latStep;
        for (uint32_t lon = 0; lon <= subdivisions; ++lon) {
            float theta = (float)lon * lonStep;
            float x = radius * std::cos(phi) * std::cos(theta);
            float y = radius * std::sin(phi);
            float z = radius * std::cos(phi) * std::sin(theta);

            Model::VertexData v;
            v.position = { x, y, z, 1.0f };
            v.normal = MatrixMath::Normalize({ x, y, z });
            v.texcoord = { (float)lon / subdivisions, (float)lat / subdivisions };
            vertices.push_back(v);
        }
    }

    for (uint32_t lat = 0; lat < subdivisions; ++lat) {
        for (uint32_t lon = 0; lon < subdivisions; ++lon) {
            uint32_t first = lat * (subdivisions + 1) + lon;
            uint32_t second = first + subdivisions + 1;
            indices.push_back(first); indices.push_back(second); indices.push_back(first + 1);
            indices.push_back(second); indices.push_back(second + 1); indices.push_back(first + 1);
        }
    }

    auto model = std::make_unique<Model>();
    model->InitializeWithData(manager, vertices, indices);
    return model;
}

/**
 * 平面の生成
 */
std::unique_ptr<Model> PrimitiveGenerator::CreatePlane(ModelManager* manager, float width, float height) {
    float hw = width * 0.5f;
    float hh = height * 0.5f;
    std::vector<Model::VertexData> vertices = {
        {{-hw, 0.0f,  hh, 1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
        {{ hw, 0.0f,  hh, 1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
        {{-hw, 0.0f, -hh, 1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}},
        {{ hw, 0.0f, -hh, 1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}},
    };
    std::vector<uint32_t> indices = { 0, 1, 2, 1, 3, 2 };
    auto model = std::make_unique<Model>();
    model->InitializeWithData(manager, vertices, indices);
    return model;
}

/**
 * 円（塗りつぶし）の生成
 */
std::unique_ptr<Model> PrimitiveGenerator::CreateCircle(ModelManager* manager, float radius, uint32_t segments) {
    std::vector<Model::VertexData> vertices;
    std::vector<uint32_t> indices;

    vertices.push_back({ {0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}, {0.5f, 0.5f} });

    const float pi = std::numbers::pi_v<float>;
    for (uint32_t i = 0; i <= segments; ++i) {
        float theta = 2.0f * pi * float(i) / float(segments);
        Model::VertexData v;
        v.position = { radius * std::cos(theta), radius * std::sin(theta), 0.0f, 1.0f };
        v.normal = { 0.0f, 0.0f, -1.0f };
        v.texcoord = { (std::cos(theta) + 1.0f) * 0.5f, (std::sin(theta) + 1.0f) * 0.5f };
        vertices.push_back(v);
    }

    for (uint32_t i = 1; i <= segments; ++i) {
        indices.push_back(0);
        indices.push_back(i);
        indices.push_back(i + 1);
    }

    auto model = std::make_unique<Model>();
    model->InitializeWithData(manager, vertices, indices);
    return model;
}

/**
 * リング（円輪）の生成
 */
std::unique_ptr<Model> PrimitiveGenerator::CreateRing(ModelManager* manager, float innerRadius, float outerRadius, uint32_t segments, float startAngle, float endAngle) {
    std::vector<Model::VertexData> vertices;
    std::vector<uint32_t> indices;

    float angleRange = endAngle - startAngle;

    for (uint32_t i = 0; i <= segments; ++i) {
        // 角度を計算
        float theta = startAngle + angleRange * (float(i) / float(segments));
        float cosT = std::cos(theta);
        float sinT = std::sin(theta);

        // ★王冠対策：円周に沿ったUV
        float u = float(i) / float(segments);

        // 外側の点 (V=0.0)
        vertices.push_back({
            {cosT * outerRadius, sinT * outerRadius, 0.0f, 1.0f},
            {0.0f, 0.0f, -1.0f},
            {u, 0.0f}
            });
        // 内側の点 (V=1.0)
        vertices.push_back({
            {cosT * innerRadius, sinT * innerRadius, 0.0f, 1.0f},
            {0.0f, 0.0f, -1.0f},
            {u, 1.0f}
            });
    }

    for (uint32_t i = 0; i < segments; ++i) {
        uint32_t base = i * 2;
        // 時計回りに三角形を作成
        indices.push_back(base);
        indices.push_back(base + 1);
        indices.push_back(base + 2);

        indices.push_back(base + 1);
        indices.push_back(base + 3);
        indices.push_back(base + 2);
    }

    auto model = std::make_unique<Model>();
    model->InitializeWithData(manager, vertices, indices);
    return model;
}

/**
 * 円柱（Cylinder）の生成 — 資料「Cylinderの拡張」完全対応版
 *
 * 【構造の説明】
 * ・エフェクト用の円柱は「側面のみ」（上面・下面のフタがない筒状）
 * ・円周方向を segments 分割、高さ方向を verticalDivisions 分割して
 *   格子状に頂点を並べ、三角形のペア（クアッド）で埋める
 *
 * 【パラメータの意味】
 *   topRadius / bottomRadius: 上面と下面で半径が違う → 円錐台（フラスタム）
 *     topRadius = 0 にすると円錐になる
 *   verticalDivisions: 高さ方向の分割数
 *     1なら上下2段だけ、増やすと途中にも頂点ができて
 *     頂点カラーのグラデーションや複雑な変形が可能になる
 *   startAngle / endAngle: 生成する角度の範囲
 *     デフォルトは 0〜2π で全周（360°）
 *     一部だけ生成すれば扇形の円柱になる
 *
 * 【UV座標の割り当て】
 *   U方向 = 円周方向（0.0 → 1.0）= 横方向UVスクロールのターゲット
 *   V方向 = 高さ方向（0.0=上 → 1.0=下）
 *
 * 【法線の計算】
 *   円柱なら法線は水平に外を向くだけだが、円錐台（上下で半径が違う）の場合
 *   法線は斜め上or下に傾く。この傾斜角（テーパー角）を反映する。
 *   テーパー角 = atan2(bottomRadius - topRadius, height)
 */
std::unique_ptr<Model> PrimitiveGenerator::CreateCylinder(
    ModelManager* manager,
    float topRadius,
    float bottomRadius,
    float height,
    uint32_t segments,
    uint32_t verticalDivisions,
    float startAngle,
    float endAngle) {

    std::vector<Model::VertexData> vertices;
    std::vector<uint32_t> indices;

    float halfH = height * 0.5f;
    // 角度の範囲（部分円柱に対応するため）
    float angleRange = endAngle - startAngle;

    // === テーパー角の計算 ===
    // 上下で半径が違う場合、側面が傾斜する
    // この傾斜の分だけ法線のY成分が変わる
    //
    //  topRadius
    //    ┌──┐      ← 上
    //     \  |  height
    //      \ |
    //    └────┘    ← 下
    //  bottomRadius
    //
    // 法線は「側面に対して垂直」なので、テーパーの分だけ上を向く
    float radiusDiff = bottomRadius - topRadius;
    float slopeLength = std::sqrt(radiusDiff * radiusDiff + height * height);
    // 法線のY成分（テーパーがなければ 0）
    float normalY = (slopeLength > 0.0f) ? (radiusDiff / slopeLength) : 0.0f;
    // 法線のXZ成分のスケール（テーパーがなければ 1）
    float normalXZ = (slopeLength > 0.0f) ? (height / slopeLength) : 1.0f;

    // === 頂点の生成 ===
    // 高さ方向: verticalDivisions + 1 段（例: divisions=1 → 上と下の2段）
    // 円周方向: segments + 1 個（始点と終点を分けてUVの継ぎ目を作る）
    for (uint32_t v = 0; v <= verticalDivisions; ++v) {
        // t: 高さ方向の補間値 (0.0=上 → 1.0=下)
        float t = static_cast<float>(v) / static_cast<float>(verticalDivisions);
        // この段の高さ（Y座標）
        float y = halfH - t * height;
        // この段の半径（上半径から下半径へ線形補間）
        float radius = topRadius + (bottomRadius - topRadius) * t;

        for (uint32_t s = 0; s <= segments; ++s) {
            // u: 円周方向の補間値 (0.0 → 1.0) = UV座標のU
            float u = static_cast<float>(s) / static_cast<float>(segments);
            // 実際の角度
            float theta = startAngle + angleRange * u;

            float cosT = std::cos(theta);
            float sinT = std::sin(theta);

            // 位置: 円周上の点に半径を掛けて配置
            float x = cosT * radius;
            float z = sinT * radius;

            // 法線: テーパー角を考慮（XZ方向は外向き、Y方向はテーパーの傾き）
            Vector3 normal = MatrixMath::Normalize({ cosT * normalXZ, normalY, sinT * normalXZ });

            Model::VertexData vertex;
            vertex.position = { x, y, z, 1.0f };
            vertex.normal = normal;
            vertex.texcoord = { u, t }; // U=円周, V=高さ
            vertices.push_back(vertex);
        }
    }

    // === インデックスの生成 ===
    // 各段のクアッド（四角形）を2つの三角形に分割して登録
    //
    //  s    s+1
    //  v ●───● v      上の段
    //    │＼  │
    //    │  ＼│
    // v+1●───● v+1   下の段
    //
    uint32_t cols = segments + 1; // 1行あたりの頂点数
    for (uint32_t v = 0; v < verticalDivisions; ++v) {
        for (uint32_t s = 0; s < segments; ++s) {
            uint32_t topLeft     = v * cols + s;
            uint32_t topRight    = v * cols + (s + 1);
            uint32_t bottomLeft  = (v + 1) * cols + s;
            uint32_t bottomRight = (v + 1) * cols + (s + 1);

            // 三角形1: 左上 → 右上 → 左下
            indices.push_back(topLeft);
            indices.push_back(topRight);
            indices.push_back(bottomLeft);

            // 三角形2: 右上 → 右下 → 左下
            indices.push_back(topRight);
            indices.push_back(bottomRight);
            indices.push_back(bottomLeft);
        }
    }

    auto model = std::make_unique<Model>();
    model->InitializeWithData(manager, vertices, indices);
    return model;
}