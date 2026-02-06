#pragma once
#include "Object3dCommon.h"
#include <string>
#include <vector>
#include "Transform.h"

class Model {
public:
    // 初期化
    void Initialize(Object3dCommon* object3dCommon);

    // 描画
    void Draw(Object3dCommon* object3dCommon);

private:
    Object3dCommon* object3dCommon_ = nullptr;

    // 頂点データ構造体
    struct VertexData {
        Vector4 position;
        Vector2 texcoord;
        Vector3 normal;
    };

    // バッファリソース
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
    VertexData* vertexData_ = nullptr;
};