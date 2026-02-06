#pragma once
#include "DirectXCommon.h" // DirectXCommonを使えるようにする
#include "Matrix.h"
#include <string>
#include <vector>
#include <wrl.h>
#include <d3d12.h>

class ModelCommon; // 前方宣言

class Model {
public:
    // 初期化: Object3dCommon ではなく ModelCommon を受け取るように変更
    void Initialize(ModelCommon* modelCommon, const std::string& directoryPath, const std::string& filename);

    // 描画: 描画には DirectXCommon さえあればOK
    void Draw(DirectXCommon* dxCommon);

    const std::string& GetTextureFilePath() const { return materialData_.textureFilePath; }

private:
    void LoadObjFile(const std::string& directoryPath, const std::string& filename);
    void LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename);

    // ModelCommonへのポインタを持つ
    ModelCommon* modelCommon_ = nullptr;

    struct VertexData {
        Vector4 position;
        Vector2 texcoord;
        Vector3 normal;
    };

    struct MaterialData {
        std::string textureFilePath;
    };

    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
    std::vector<VertexData> vertices_;

    MaterialData materialData_;
};