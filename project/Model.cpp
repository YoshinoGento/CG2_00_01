#include "Model.h"
#include <cassert>

void Model::Initialize(Object3dCommon* object3dCommon) {
    object3dCommon_ = object3dCommon;
    DirectXCommon* dxCommon = object3dCommon_->GetDxCommon();

    // 1. 頂点バッファの作成 (三角形用の3頂点)
    vertexResource_ = dxCommon->CreateBufferResource(sizeof(VertexData) * 3);

    // 2. 頂点バッファビューの設定
    vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = sizeof(VertexData) * 3;
    vertexBufferView_.StrideInBytes = sizeof(VertexData);

    // 3. 頂点データの書き込み
    vertexResource_->Map(0, nullptr, (void**)&vertexData_);

    // 仮のデータ：三角形
    // 左下
    vertexData_[0].position = { -0.5f, -0.5f, 0.0f, 1.0f };
    vertexData_[0].texcoord = { 0.0f, 1.0f };
    vertexData_[0].normal = { 0.0f, 0.0f, -1.0f };
    // 上
    vertexData_[1].position = { 0.0f, 0.5f, 0.0f, 1.0f };
    vertexData_[1].texcoord = { 0.5f, 0.0f };
    vertexData_[1].normal = { 0.0f, 0.0f, -1.0f };
    // 右下
    vertexData_[2].position = { 0.5f, -0.5f, 0.0f, 1.0f };
    vertexData_[2].texcoord = { 1.0f, 1.0f };
    vertexData_[2].normal = { 0.0f, 0.0f, -1.0f };
}

void Model::Draw(Object3dCommon* object3dCommon) {
    // 描画コマンドの発行
    ID3D12GraphicsCommandList* commandList = object3dCommon->GetDxCommon()->GetCommandList();

    // 頂点バッファをセット
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);

    // 描画 (3頂点)
    commandList->DrawInstanced(3, 1, 0, 0);
}