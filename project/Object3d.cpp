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
	Matrix4x4 scaleMatrix = MatrixMath::MakeScaleMatrix(transform_.scale);
	Matrix4x4 rotateMatrix = MatrixMath::Multiply(MatrixMath::MakeRotateXMatrix(transform_.rotate.x),
		MatrixMath::Multiply(MatrixMath::MakeRotateYMatrix(transform_.rotate.y),
			MatrixMath::MakeRotateZMatrix(transform_.rotate.z)));
	Matrix4x4 translateMatrix = MatrixMath::MakeTranslateMatrix(transform_.translate);
	Matrix4x4 worldMatrix = MatrixMath::Multiply(scaleMatrix, MatrixMath::Multiply(rotateMatrix, translateMatrix));

	if (camera) {
		transformationMatrixData_->WVP = MatrixMath::Multiply(worldMatrix, camera->GetViewProjectionMatrix());
		cameraData_->worldPosition = camera->GetTranslate();
	} else {
		transformationMatrixData_->WVP = worldMatrix;
	}
	transformationMatrixData_->World = worldMatrix;
}

void Object3d::Draw() {
	if (!model_) return;
	ID3D12GraphicsCommandList* commandList = object3dCommon_->GetDxCommon()->GetCommandList();

	commandList->SetPipelineState(object3dCommon_->GetPipelineState(cullMode_));

	commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(1, transformationMatrixResource_->GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(2, directionalLightResource_->GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(3, cameraResource_->GetGPUVirtualAddress());

	// ★重要：スポットライトは [4] 番
	commandList->SetGraphicsRootConstantBufferView(4, spotLightResource_->GetGPUVirtualAddress());

	// ★重要：テクスチャは [5] 番
	SrvManager* srvManager = object3dCommon_->GetSrvManager();
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle = srvManager->GetGPUDescriptorHandle(textureHandle_);
	commandList->SetGraphicsRootDescriptorTable(5, textureSrvHandle);

	model_->Draw(object3dCommon_->GetDxCommon());
}

void Object3d::SetModel(Model* model) { model_ = model; }