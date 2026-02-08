#include "Sprite.h"

void Sprite::Initialize(SpriteCommon* spriteCommon, uint32_t textureHandle) {
    assert(spriteCommon);
    spriteCommon_ = spriteCommon;
    textureHandle_ = textureHandle; // テクスチャハンドルを保存

    // 1. 頂点バッファの作成
    vertexResource_ = spriteCommon_->GetDxCommon()->CreateBufferResource(sizeof(VertexData) * 4);
    vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = sizeof(VertexData) * 4;
    vertexBufferView_.StrideInBytes = sizeof(VertexData);
    vertexResource_->Map(0, nullptr, (void**)&vertexData_);

    // 2. インデックスバッファの作成
    indexResource_ = spriteCommon_->GetDxCommon()->CreateBufferResource(sizeof(uint32_t) * 6);
    indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
    indexBufferView_.SizeInBytes = sizeof(uint32_t) * 6;
    indexBufferView_.Format = DXGI_FORMAT_R32_UINT;
    indexResource_->Map(0, nullptr, (void**)&indexData_);

    indexData_[0] = 0; indexData_[1] = 1; indexData_[2] = 2;
    indexData_[3] = 1; indexData_[4] = 3; indexData_[5] = 2;

    // 3. マテリアル
    materialResource_ = spriteCommon_->GetDxCommon()->CreateBufferResource(sizeof(Material));
    materialResource_->Map(0, nullptr, (void**)&materialData_);
    materialData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    materialData_->enableLighting = 0;
    materialData_->uvTransform = MatrixMath::MakeIdentity4x4();

    // 4. 座標変換行列
    transformationMatrixResource_ = spriteCommon_->GetDxCommon()->CreateBufferResource(sizeof(TransformationMatrix));
    transformationMatrixResource_->Map(0, nullptr, (void**)&transformationMatrixData_);
    transformationMatrixData_->WVP = MatrixMath::MakeIdentity4x4();
    transformationMatrixData_->World = MatrixMath::MakeIdentity4x4();

    // 画像サイズに合わせてスプライトサイズを初期化
    AdjustTextureRect();
}

void Sprite::Update() {
    // スプライトのアンカーポイントなどを考慮して座標変換行列を計算
    Matrix4x4 rotateMatrix = MatrixMath::MakeRotateZMatrix(rotation_);
    Matrix4x4 translateMatrix = MatrixMath::MakeTranslateMatrix({ position_.x, position_.y, 0.0f });
    Matrix4x4 scaleMatrix = MatrixMath::MakeScaleMatrix({ size_.x, size_.y, 1.0f });

    // 原点中心に回転させる場合など、順番は要調整だが、基本は Scale -> Rotate -> Translate
    Matrix4x4 worldMatrix = MatrixMath::Multiply(scaleMatrix, MatrixMath::Multiply(rotateMatrix, translateMatrix));

    // ビュー・プロジェクション（平行投影）
    Matrix4x4 viewMatrix = MatrixMath::MakeIdentity4x4();
    Matrix4x4 projectionMatrix = MatrixMath::MakeOrthographicMatrix(0.0f, 0.0f, 1280.0f, 720.0f, 0.0f, 100.0f);

    Matrix4x4 worldViewProjectionMatrix = MatrixMath::Multiply(worldMatrix, MatrixMath::Multiply(viewMatrix, projectionMatrix));

    transformationMatrixData_->World = worldMatrix;
    transformationMatrixData_->WVP = worldViewProjectionMatrix;
}

void Sprite::Draw() {
    ID3D12GraphicsCommandList* commandList = spriteCommon_->GetDxCommon()->GetCommandList();

    // 頂点・インデックスセット
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
    commandList->IASetIndexBuffer(&indexBufferView_);

    // ルートパラメータセット
    // 0: Material
    commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
    // 1: TransformationMatrix
    commandList->SetGraphicsRootConstantBufferView(1, transformationMatrixResource_->GetGPUVirtualAddress());

    // 2: Texture (DescriptorTable)
    // ★ここを修正しました！引数を2つ（ルートパラメータ番号, ハンドル）にしました
    D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle = spriteCommon_->GetSrvHandleGPU(textureHandle_);
    commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandle);

    // 描画
    commandList->DrawIndexedInstanced(6, 1, 0, 0, 0);
}

void Sprite::SetTexture(uint32_t textureHandle) {
    textureHandle_ = textureHandle;
    AdjustTextureRect();
}

void Sprite::SetTextureRect(const Vector2& leftTop, const Vector2& size) {
    // UV変換行列の計算（指定範囲を切り出す）
    // ...実装省略（uvTransformを更新する）...
}

void Sprite::AdjustTextureRect() {
    // テクスチャのサイズを取得してスプライトサイズに反映
    D3D12_RESOURCE_DESC resDesc = spriteCommon_->GetTextureResourceDesc(textureHandle_);
    size_ = { (float)resDesc.Width, (float)resDesc.Height };
}