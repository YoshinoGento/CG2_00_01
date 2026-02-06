#pragma once
#include "Object3dCommon.h"
#include "Model.h" // Modelの定義が必要
#include "Matrix.h"

class Object3d {
public:
    // 初期化 (共通部を受け取る)
    void Initialize(Object3dCommon* object3dCommon);

    // 更新
    void Update();

    // 描画
    void Draw();

    // ★追加: モデルセット
    void SetModel(const std::string& filename);

    // ゲッター・セッター
    const Vector3& GetPosition() const { return transform_.translate; }
    const Vector3& GetRotation() const { return transform_.rotate; }
    const Vector3& GetScale() const { return transform_.scale; }

    void SetPosition(const Vector3& position) { transform_.translate = position; }
    void SetRotation(const Vector3& rotation) { transform_.rotate = rotation; }
    void SetScale(const Vector3& scale) { transform_.scale = scale; }

    void SetTexture(uint32_t textureHandle) { textureHandle_ = textureHandle; }

private:
    Object3dCommon* object3dCommon_ = nullptr;

    // ★追加: モデルデータへのポインタ
    Model* model_ = nullptr;

    uint32_t textureHandle_ = 0;

    struct Material {
        Vector4 color;
        int32_t enableLighting;
        float padding[3];
        Matrix4x4 uvTransform;
    };

    struct TransformationMatrix {
        Matrix4x4 WVP;
        Matrix4x4 World;
    };

    struct DirectionalLight {
        Vector4 color;
        Vector3 direction;
        float intensity;
    };

    Transform transform_ = { {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };

    // バッファリソース
    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
    Material* materialData_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResource_;
    TransformationMatrix* transformationMatrixData_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource_;
    DirectionalLight* directionalLightData_ = nullptr;
};