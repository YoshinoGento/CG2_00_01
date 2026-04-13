#include "PrimitiveGenerator.h"
#include "Matrix.h"
#include <numbers>

/**
 * 立方体の生成
 */
std::unique_ptr<Model> PrimitiveGenerator::CreateBox(ModelManager* manager, const Vector3& size) {
    float hx = size.x * 0.5f;
    float hy = size.y * 0.5f;
    float hz = size.z * 0.5f;

    // 立方体の24頂点（各面4点ずつ：法線を分けるため）
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
        {{ hx, -hy, -hz, 1.0f}, {1, 0,  0}, {0, 1}}, {{ hx, -hy,  hz, 1.0f}, {1, 0,  0}, {1, 1}},
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
    // Modelクラスに「データから初期化する」関数が必要（後述）
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

    // 頂点の生成
    for (uint32_t lat = 0; lat <= subdivisions; ++lat) {
        float phi = pi / 2.0f - (float)lat * latStep; // 緯度
        for (uint32_t lon = 0; lon <= subdivisions; ++lon) {
            float theta = (float)lon * lonStep; // 経度

            float x = radius * std::cos(phi) * std::cos(theta);
            float y = radius * std::sin(phi);
            float z = radius * std::cos(phi) * std::sin(theta);

            Model::VertexData v;
            v.position = { x, y, z, 1.0f };
            v.normal = MatrixMath::Normalize({ x, y, z }); // 球体なので座標=法線の方向
            v.texcoord = { (float)lon / subdivisions, (float)lat / subdivisions };
            vertices.push_back(v);
        }
    }

    // インデックスの生成（グリッド状につなぐ）
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