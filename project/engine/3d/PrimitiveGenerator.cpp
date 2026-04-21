#include "3d/PrimitiveGenerator.h"
#include "math/Matrix.h"
#include <numbers>
#include <cmath>

/**
 * 立方体の生成
 */
std::unique_ptr<Model> PrimitiveGenerator::CreateBox(ModelManager* manager, const Vector3& size) {
    float hx = size.x * 0.5f;
    float hy = size.y * 0.5f;
    float hz = size.z * 0.5f;

    // 立方体の24頂点（各面4点ずつ：法線を分けるため）
    // ★Model.h の定義に合わせて「position」を使用します
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
        {{-hx, -hy,  hz, 1.0f}, {0, -1, 0}, {0, 1}}, {{ hx, -hy,  hz, 1.0f}, {0, -1, 0}, {1, 1}},
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
            // ★修正：v.pos ではなく v.position を使用
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
    // ★ここも position に合わせます
    std::vector<Model::VertexData> vertices = {
        {{-hw, 0.0f,  hh, 1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}}, // 左上
        {{ hw, 0.0f,  hh, 1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}}, // 右上
        {{-hw, 0.0f, -hh, 1.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}}, // 左下
        {{ hw, 0.0f, -hh, 1.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}}, // 右下
    };
    std::vector<uint32_t> indices = { 0, 1, 2, 1, 3, 2 };
    auto model = std::make_unique<Model>();
    model->InitializeWithData(manager, vertices, indices);
    return model;
}

/**
 * 円の生成
 */
std::unique_ptr<Model> PrimitiveGenerator::CreateCircle(ModelManager* manager, float radius, uint32_t segments) {
    std::vector<Model::VertexData> vertices;
    std::vector<uint32_t> indices;

    // 中心点
    vertices.push_back({ {0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, -1.0f}, {0.5f, 0.5f} });

    const float pi = std::numbers::pi_v<float>;
    for (uint32_t i = 0; i <= segments; ++i) {
        float theta = 2.0f * pi * float(i) / float(segments);
        Model::VertexData v;
        // ★修正：v.pos ではなく v.position を使用
        v.position = { radius * std::cos(theta), radius * std::sin(theta), 0.0f, 1.0f };
        v.normal = { 0.0f, 0.0f, -1.0f };
        v.texcoord = { (std::cos(theta) + 1.0f) * 0.5f, (std::sin(theta) + 1.0f) * 0.5f };
        vertices.push_back(v);

        if (i > 0) {
            indices.push_back(0);
            indices.push_back(i);
            indices.push_back(i + 1);
        }
    }

    auto model = std::make_unique<Model>();
    model->InitializeWithData(manager, vertices, indices);
    return model;
}