#include "Object3d.h"
#include "Object3dCommon.h"
#include "SrvManager.h"
#include "DirectXCommon.h"
#include <cassert>

void Object3d::Initialize(Object3dCommon* object3dCommon) {
	assert(object3dCommon);
	object3dCommon_ = object3dCommon;
	DirectXCommon* dxCommon = object3dCommon_->GetDxCommon();

	materialResource_ = dxCommon->CreateBufferResource(sizeof(Material));
	materialResource_->Map(0, nullptr, (void**)&materialData_);
	materialData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	materialData_->enableLighting = 1;
	materialData_->shininess = 40.0f;
	materialData_->environmentCoefficient = 0.0f;
	materialData_->uvTransform = MatrixMath::MakeIdentity4x4();

	transformationMatrixResource_ = dxCommon->CreateBufferResource(sizeof(TransformationMatrix));
	transformationMatrixResource_->Map(0, nullptr, (void**)&transformationMatrixData_);

	directionalLightResource_ = dxCommon->CreateBufferResource(sizeof(DirectionalLight));
	directionalLightResource_->Map(0, nullptr, (void**)&directionalLightData_);
	directionalLightData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	directionalLightData_->direction = { 0.0f, -1.0f, 1.0f };
	directionalLightData_->intensity = 1.0f;

	cameraResource_ = dxCommon->CreateBufferResource(sizeof(CameraForGPU));
	cameraResource_->Map(0, nullptr, (void**)&cameraData_);

	spotLightResource_ = dxCommon->CreateBufferResource(sizeof(SpotLightData));
	spotLightResource_->Map(0, nullptr, (void**)&spotLightData_);
	spotLightData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	spotLightData_->position = { 0.0f, 4.0f, 0.0f };
	spotLightData_->intensity = 0.0f;
	spotLightData_->direction = { 0.0f, -1.0f, 0.0f };
	spotLightData_->distance = 10.0f;
	spotLightData_->decay = 1.0f;
	spotLightData_->cosAngle = std::cos(0.5f);
	spotLightData_->cosFalloffStart = std::cos(0.3f);
}

void Object3d::Update(Camera* camera) {
	// 1. オブジェクト自身の変形行列を作る
	Matrix4x4 scaleMatrix = MatrixMath::MakeScaleMatrix(transform_.scale);
	Matrix4x4 rotateMatrix = MatrixMath::Multiply(MatrixMath::MakeRotateXMatrix(transform_.rotate.x),
		MatrixMath::Multiply(MatrixMath::MakeRotateYMatrix(transform_.rotate.y),
			MatrixMath::MakeRotateZMatrix(transform_.rotate.z)));
	Matrix4x4 translateMatrix = MatrixMath::MakeTranslateMatrix(transform_.translate);
	Matrix4x4 worldMatrix = MatrixMath::Multiply(scaleMatrix, MatrixMath::Multiply(rotateMatrix, translateMatrix));

	// 2. モデル内の階層構造（ノード）の行列を合成する (資料スライド 10)
	// モデルが持つ RootNode の LocalMatrix を、オブジェクトの WorldMatrix に掛け合わせる
	if (model_) {
		const Model::Node& rootNode = model_->GetRootNode();
		worldMatrix = MatrixMath::Multiply(rootNode.localMatrix, worldMatrix);
	}

	// 3. 定数バッファへの書き込み
	if (camera) {
		transformationMatrixData_->WVP = MatrixMath::Multiply(worldMatrix, camera->GetViewProjectionMatrix());
		cameraData_->worldPosition = camera->GetTranslate();
	}
	else {
		transformationMatrixData_->WVP = worldMatrix;
	}
	transformationMatrixData_->World = worldMatrix;
}

void Object3d::Draw() {
	if (!model_) return;
	ID3D12GraphicsCommandList* commandList = object3dCommon_->GetDxCommon()->GetCommandList();

	// パイプラインをセット（カリング設定含む）
	commandList->SetPipelineState(object3dCommon_->GetPipelineState(cullMode_));

	// 各定数バッファを RootSignature の Index に合わせてセット
	// 0: Material (Pixel)
	commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
	// 1: Transform (Vertex)
	commandList->SetGraphicsRootConstantBufferView(1, transformationMatrixResource_->GetGPUVirtualAddress());
	// 2: Directional Light (Pixel)
	commandList->SetGraphicsRootConstantBufferView(2, directionalLightResource_->GetGPUVirtualAddress());
	// 3: Camera (Pixel)
	commandList->SetGraphicsRootConstantBufferView(3, cameraResource_->GetGPUVirtualAddress());
	// ★重要：4番はスポットライト (CBV)
	commandList->SetGraphicsRootConstantBufferView(4, spotLightResource_->GetGPUVirtualAddress());

	// ★重要：5番がテクスチャ (Descriptor Table)
	// 地形（Terrain）の場合は Model::Draw 内で上書きされますが、念のためここでも正しい番号を指定します
	SrvManager* srvManager = object3dCommon_->GetSrvManager();
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle = srvManager->GetGPUDescriptorHandle(textureHandle_);
	commandList->SetGraphicsRootDescriptorTable(5, textureSrvHandle);

	// ★追加：環境マップ用のテクスチャ (Descriptor Table)
	D3D12_GPU_DESCRIPTOR_HANDLE environmentMapSrvHandle = srvManager->GetGPUDescriptorHandle(environmentMapHandle_);
	commandList->SetGraphicsRootDescriptorTable(6, environmentMapSrvHandle);
	// 環境マップ (t1) ★追加
	commandList->SetGraphicsRootDescriptorTable(6, srvManager->GetGPUDescriptorHandle(environmentMapHandle_));

	// モデルの描画実行
	model_->Draw(object3dCommon_->GetDxCommon());
}
void Object3d::SetModel(Model* model) { model_ = model; }