#include "Object3d.h"
#include <cassert>

// 初期化
void Object3d::Initialize(Object3dCommon* object3dCommon) {
    assert(object3dCommon);
    object3dCommon_ = object3dCommon;
    DirectXCommon* dxCommon = object3dCommon_->GetDxCommon();

    // ★頂点バッファの作成処理は Model へ移動したので削除しました

    // 1. マテリアルバッファの作成
    materialResource_ = dxCommon->CreateBufferResource(sizeof(Material));
    materialResource_->Map(0, nullptr, (void**)&materialData_);

    materialData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    materialData_->enableLighting = 1;
    materialData_->uvTransform = MatrixMath::MakeIdentity4x4();

    // 2. 座標変換行列バッファの作成
    transformationMatrixResource_ = dxCommon->CreateBufferResource(sizeof(TransformationMatrix));
    transformationMatrixResource_->Map(0, nullptr, (void**)&transformationMatrixData_);

    transformationMatrixData_->WVP = MatrixMath::MakeIdentity4x4();
    transformationMatrixData_->World = MatrixMath::MakeIdentity4x4();

    // 3. 平行光源バッファの作成
    directionalLightResource_ = dxCommon->CreateBufferResource(sizeof(DirectionalLight));
    directionalLightResource_->Map(0, nullptr, (void**)&directionalLightData_);

    directionalLightData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    directionalLightData_->direction = { 0.0f, -1.0f, 0.0f };
    directionalLightData_->intensity = 1.0f;
}

// ★追加: モデルセット
void Object3d::SetModel(const std::string& filename) {
    // 共通部からモデルをもらってくる
    model_ = object3dCommon_->GetModel(filename);
}

// 更新
void Object3d::Update() {
    Matrix4x4 scaleMatrix = MatrixMath::MakeScaleMatrix(transform_.scale);
    Matrix4x4 rotateMatrixX = MatrixMath::MakeRotateXMatrix(transform_.rotate.x);
    Matrix4x4 rotateMatrixY = MatrixMath::MakeRotateYMatrix(transform_.rotate.y);
    Matrix4x4 rotateMatrixZ = MatrixMath::MakeRotateZMatrix(transform_.rotate.z);
    Matrix4x4 rotateMatrix = MatrixMath::Multiply(rotateMatrixX, MatrixMath::Multiply(rotateMatrixY, rotateMatrixZ));
    Matrix4x4 translateMatrix = MatrixMath::MakeTranslateMatrix(transform_.translate);

    Matrix4x4 worldMatrix = MatrixMath::Multiply(scaleMatrix, MatrixMath::Multiply(rotateMatrix, translateMatrix));

    // ビュー・プロジェクション行列（WinAppのサイズを使う）
    Matrix4x4 viewMatrix = MatrixMath::MakeIdentity4x4();
    Matrix4x4 projectionMatrix = MatrixMath::MakePerspectiveFovMatrix(0.45f, 1280.0f / 720.0f, 0.1f, 100.0f);
    Matrix4x4 worldViewProjectionMatrix = MatrixMath::Multiply(worldMatrix, MatrixMath::Multiply(viewMatrix, projectionMatrix));

    transformationMatrixData_->World = worldMatrix;
    transformationMatrixData_->WVP = worldViewProjectionMatrix;
}

// 描画
void Object3d::Draw() {
    // モデルがセットされていなければ描画しない
    if (!model_) return;

    ID3D12GraphicsCommandList* commandList = object3dCommon_->GetDxCommon()->GetCommandList();

    // ルートパラメータの設定
    commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(1, transformationMatrixResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(2, directionalLightResource_->GetGPUVirtualAddress());

    // テクスチャ設定 (簡易)
    D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle = object3dCommon_->GetDxCommon()->GetSRVGPUDescriptorHandle(textureHandle_);
    commandList->SetGraphicsRootDescriptorTable(3, textureSrvHandle);

    // ★モデルに描画してもらう
    model_->Draw(object3dCommon_);
}