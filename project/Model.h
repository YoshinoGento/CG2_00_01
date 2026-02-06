#pragma once
#include "Object3dCommon.h"
#include "Matrix.h" // Vector4などの定義が必要
#include <string>
#include <vector>

class Model {
public:
    // 初期化 (ディレクトリパスとファイル名を受け取るように変更)
    void Initialize(Object3dCommon* object3dCommon, const std::string& directoryPath, const std::string& filename);

    // 描画
    void Draw(Object3dCommon* object3dCommon);

private:
    // OBJファイル読み込み関数
    void LoadObjFile(const std::string& directoryPath, const std::string& filename);

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

    // 頂点データ（読み込んだデータをここに貯める）
    std::vector<VertexData> vertices_;
};