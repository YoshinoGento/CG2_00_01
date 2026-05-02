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
 * 円柱の生成
 */
std::unique_ptr<Model> PrimitiveGenerator::CreateCylinder(ModelManager* manager, float radius, float height, uint32_t segments) {
    std::vector<Model::VertexData> vertices;
    std::vector<uint32_t> indices;

    const float pi = std::numbers::pi_v<float>;
    float halfH = height * 0.5f;

    for (uint32_t i = 0; i <= segments; ++i) {
        float theta = 2.0f * pi * float(i) / float(segments);
        float x = std::cos(theta);
        float z = std::sin(theta);

        vertices.push_back({ {x * radius,  halfH, z * radius, 1.0f}, {x, 0, z}, {(float)i / segments, 0} });
        vertices.push_back({ {x * radius, -halfH, z * radius, 1.0f}, {x, 0, z}, {(float)i / segments, 1} });
    }

    for (uint32_t i = 0; i < segments; ++i) {
        uint32_t base = i * 2;
        indices.push_back(base); indices.push_back(base + 2); indices.push_back(base + 1);
        indices.push_back(base + 1); indices.push_back(base + 2); indices.push_back(base + 3);
    }

    auto model = std::make_unique<Model>();
    model->InitializeWithData(manager, vertices, indices);
    return model;
}