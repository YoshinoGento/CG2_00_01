#include "Sprite.h"
#include <cassert>

void Sprite::Initialize(SpriteCommon* spriteCommon, uint32_t textureHandle) {
    assert(spriteCommon);
    spriteCommon_ = spriteCommon;
    textureHandle_ = textureHandle;

    vertexResource_ = spriteCommon_->GetDxCommon()->CreateBufferResource(sizeof(VertexData) * 4);
    vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = sizeof(VertexData) * 4;
    vertexBufferView_.StrideInBytes = sizeof(VertexData);
    vertexResource_->Map(0, nullptr, (void**)&vertexData_);

    // ★頂点データの設定（四角形の形を確定させる）
    vertexData_[0].position = { 0.0f, 1.0f, 0.0f, 1.0f };
    vertexData_[0].texcoord = { 0.0f, 1.0f, 0.0f, 0.0f };
    vertexData_[1].position = { 0.0f, 0.0f, 0.0f, 1.0f };
    vertexData_[1].texcoord = { 0.0f, 0.0f, 0.0f, 0.0f };
    vertexData_[2].position = { 1.0f, 1.0f, 0.0f, 1.0f };
    vertexData_[2].texcoord = { 1.0f, 1.0f, 0.0f, 0.0f };
    vertexData_[3].position = { 1.0f, 0.0f, 0.0f, 1.0f };
    vertexData_[3].texcoord = { 1.0f, 0.0f, 0.0f, 0.0f };

    indexResource_ = spriteCommon_->GetDxCommon()->CreateBufferResource(sizeof(uint32_t) * 6);
    indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
    indexBufferView_.SizeInBytes = sizeof(uint32_t) * 6;
    indexBufferView_.Format = DXGI_FORMAT_R32_UINT;
    indexResource_->Map(0, nullptr, (void**)&indexData_);
    indexData_[0] = 0; indexData_[1] = 1; indexData_[2] = 2;
    indexData_[3] = 1; indexData_[4] = 3; indexData_[5] = 2;

    materialResource_ = spriteCommon_->GetDxCommon()->CreateBufferResource(sizeof(Material));
    materialResource_->Map(0, nullptr, (void**)&materialData_);
    materialData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    materialData_->enableLighting = 0;
    materialData_->uvTransform = MatrixMath::MakeIdentity4x4();

    transformationMatrixResource_ = spriteCommon_->GetDxCommon()->CreateBufferResource(sizeof(TransformationMatrix));
    transformationMatrixResource_->Map(0, nullptr, (void**)&transformationMatrixData_);

    AdjustTextureRect();
}

void Sprite::Update() {
    Matrix4x4 scaleMatrix = MatrixMath::MakeScaleMatrix({ size_.x, size_.y, 1.0f });
    Matrix4x4 rotateMatrix = MatrixMath::MakeRotateZMatrix(rotation_);
    Matrix4x4 translateMatrix = MatrixMath::MakeTranslateMatrix({ position_.x, position_.y, 0.0f });
    Matrix4x4 worldMatrix = MatrixMath::Multiply(scaleMatrix, MatrixMath::Multiply(rotateMatrix, translateMatrix));

    Matrix4x4 viewMatrix = MatrixMath::MakeIdentity4x4();
    Matrix4x4 projectionMatrix = MatrixMath::MakeOrthographicMatrix(0.0f, 0.0f, 1280.0f, 720.0f, 0.0f, 100.0f);

    transformationMatrixData_->World = worldMatrix;
    transformationMatrixData_->WVP = MatrixMath::Multiply(worldMatrix, MatrixMath::Multiply(viewMatrix, projectionMatrix));
}

void Sprite::Draw() {
    ID3D12GraphicsCommandList* commandList = spriteCommon_->GetDxCommon()->GetCommandList();
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
    commandList->IASetIndexBuffer(&indexBufferView_);
    commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(1, transformationMatrixResource_->GetGPUVirtualAddress());
    D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle = spriteCommon_->GetSrvHandleGPU(textureHandle_);
    commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandle);
    commandList->DrawIndexedInstanced(6, 1, 0, 0, 0);
}

void Sprite::SetTexture(uint32_t textureHandle) {
    textureHandle_ = textureHandle;
    AdjustTextureRect();
}

void Sprite::AdjustTextureRect() {
    D3D12_RESOURCE_DESC resDesc = spriteCommon_->GetTextureResourceDesc(textureHandle_);
    size_ = { (float)resDesc.Width, (float)resDesc.Height };
}