#include "Object3d.h"
#include <cassert>

void Object3d::Initialize(Object3dCommon* object3dCommon) {
	assert(object3dCommon);
	object3dCommon_ = object3dCommon;
	DirectXCommon* dxCommon = object3dCommon_->GetDxCommon();

	materialResource_ = dxCommon->CreateBufferResource(sizeof(Material));
	materialResource_->Map(0, nullptr, (void**)&materialData_);

	materialData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	materialData_->enableLighting = 1;
	materialData_->uvTransform = MatrixMath::MakeIdentity4x4();

	transformationMatrixResource_ = dxCommon->CreateBufferResource(sizeof(TransformationMatrix));
	transformationMatrixResource_->Map(0, nullptr, (void**)&transformationMatrixData_);

	transformationMatrixData_->WVP = MatrixMath::MakeIdentity4x4();
	transformationMatrixData_->World = MatrixMath::MakeIdentity4x4();

	directionalLightResource_ = dxCommon->CreateBufferResource(sizeof(DirectionalLight));
	directionalLightResource_->Map(0, nullptr, (void**)&directionalLightData_);

	directionalLightData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	directionalLightData_->direction = { 0.0f, -1.0f, 1.0f };
	directionalLightData_->intensity = 1.0f;
}

void Object3d::SetModel(Model* model) {
	model_ = model;
}

// ★変更: 引数でカメラを受け取り、その行列を使ってWVPを計算する
void Object3d::Update(Camera* camera) {
	Matrix4x4 scaleMatrix = MatrixMath::MakeScaleMatrix(transform_.scale);
	Matrix4x4 rotateMatrixX = MatrixMath::MakeRotateXMatrix(transform_.rotate.x);
	Matrix4x4 rotateMatrixY = MatrixMath::MakeRotateYMatrix(transform_.rotate.y);
	Matrix4x4 rotateMatrixZ = MatrixMath::MakeRotateZMatrix(transform_.rotate.z);
	Matrix4x4 rotateMatrix = MatrixMath::Multiply(rotateMatrixX, MatrixMath::Multiply(rotateMatrixY, rotateMatrixZ));
	Matrix4x4 translateMatrix = MatrixMath::MakeTranslateMatrix(transform_.translate);

	Matrix4x4 worldMatrix = MatrixMath::Multiply(scaleMatrix, MatrixMath::Multiply(rotateMatrix, translateMatrix));

	// ★カメラがあればその行列を使う
	Matrix4x4 worldViewProjectionMatrix;
	if (camera) {
		const Matrix4x4& viewProjectionMatrix = camera->GetViewProjectionMatrix();
		worldViewProjectionMatrix = MatrixMath::Multiply(worldMatrix, viewProjectionMatrix);
	} else {
		// カメラがない場合はとりあえず単位行列 (映らない可能性大)
		worldViewProjectionMatrix = worldMatrix;
	}

	transformationMatrixData_->World = worldMatrix;
	transformationMatrixData_->WVP = worldViewProjectionMatrix;
}

void Object3d::Draw() {
	if (!model_) return;

	ID3D12GraphicsCommandList* commandList = object3dCommon_->GetDxCommon()->GetCommandList();

	commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(1, transformationMatrixResource_->GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(2, directionalLightResource_->GetGPUVirtualAddress());

	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle = object3dCommon_->GetDxCommon()->GetSRVGPUDescriptorHandle(textureHandle_);
	commandList->SetGraphicsRootDescriptorTable(3, textureSrvHandle);

	model_->Draw(object3dCommon_->GetDxCommon());
}