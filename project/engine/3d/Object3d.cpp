#include "3d/Object3d.h"
#include "3d/Object3dCommon.h"
#include "base/SrvManager.h"
#include "base/DirectXCommon.h"
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
	shadowTransformationMatrixResource_ = dxCommon->CreateBufferResource(sizeof(TransformationMatrix));
	shadowTransformationMatrixResource_->Map(0, nullptr, (void**)&shadowTransformationMatrixData_);

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
	assert(camera);
	computeSkinningPrepared_ = false;

	if (isAnimationPlaying_ && animation_.duration > 0.0f) {
		animationTime_ += (1.0f / 60.0f) * animationSpeed_;
		animationTime_ = std::fmod(animationTime_, animation_.duration);
		if (animationTime_ < 0.0f) {
			animationTime_ += animation_.duration;
		}

		// --- フェーズ1：アニメーションの適用 ---
		// 手動コントロールフラグがオフのときのみアニメーションを骨に適用する
		if (skeleton_ && !isBoneManualControl_) {
			SkeletonSystem::ApplyAnimation(*skeleton_, animation_, animationTime_);
		}
	}

	// --- フェーズ2：スケルトンの更新 ---
	// 手動で書き換えた骨のTransformに基づいて、行列階層を正しく再計算する
	if (skeleton_) {
		SkeletonSystem::Update(*skeleton_);
		if (model_ && model_->HasSkinCluster()) {
			model_->UpdateSkinCluster(*skeleton_);
		}
	}

	// --- 1. オブジェクト自身の変形行列を作る ---
	Matrix4x4 scaleMatrix = MatrixMath::MakeScaleMatrix(transform_.scale);
	Matrix4x4 rotateMatrix = MatrixMath::Multiply(MatrixMath::MakeRotateXMatrix(transform_.rotate.x),
		MatrixMath::Multiply(MatrixMath::MakeRotateYMatrix(transform_.rotate.y),
			MatrixMath::MakeRotateZMatrix(transform_.rotate.z)));
	Matrix4x4 translateMatrix = MatrixMath::MakeTranslateMatrix(transform_.translate);
	// ★修正：ルートノードのローカル行列を含まない、オブジェクト自身の変換行列を保持する
	objectWorldMatrix_ = MatrixMath::Multiply(scaleMatrix, MatrixMath::Multiply(rotateMatrix, translateMatrix));
	worldMatrix_ = objectWorldMatrix_;

	// --- 2. モデル内の階層構造（ノード）の行列を合成する ---
	if (model_ && skeleton_) {
		const Model::Node &rootNode = model_->GetRootNode();
		worldMatrix_ = MatrixMath::Multiply(rootNode.localMatrix, worldMatrix_);
	}

	// --- 3. 定数バッファへの書き込み ---
	const Matrix4x4& matrixForRendering = (model_ && model_->HasSkinCluster()) ? objectWorldMatrix_ : worldMatrix_;
	transformationMatrixData_->WVP = MatrixMath::Multiply(matrixForRendering, camera->GetViewProjectionMatrix());
	transformationMatrixData_->World = matrixForRendering;

	// カメラ座標の更新
	cameraData_->worldPosition = camera->GetTranslate();

	// --- 4. スポットライトの更新 (正規化の追加) ---
	// ImGuiなどで方向が変更された場合、計算に使う前に必ず長さを 1 にします
	// これを行わないと、シェーダー側で角度による減衰計算が破綻します
	spotLightData_->direction = MatrixMath::Normalize(spotLightData_->direction);
}

void Object3d::Draw() {
	if (!model_) return;
	ID3D12GraphicsCommandList* commandList = object3dCommon_->GetDxCommon()->GetCommandList();
	const bool isSkinned = model_->HasSkinCluster();
	const bool useComputeSkinning = model_->UseComputeSkinning();
	const bool useVertexShaderSkinning = isSkinned && !useComputeSkinning;

	if (useComputeSkinning && !computeSkinningPrepared_) {
		model_->DispatchComputeSkinning(object3dCommon_);
		computeSkinningPrepared_ = true;
	}

	// 3Dオブジェクト用のルートシグネチャを明示的にセットする
	commandList->SetGraphicsRootSignature(object3dCommon_->GetRootSignature(useVertexShaderSkinning));

	// パイプラインをセット
	commandList->SetPipelineState(object3dCommon_->GetPipelineState(cullMode_, useVertexShaderSkinning));

	// RootSignature の Index に合わせて定数バッファをセット
	commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(1, transformationMatrixResource_->GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(2, directionalLightResource_->GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(3, cameraResource_->GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(4, spotLightResource_->GetGPUVirtualAddress());

	SrvManager* srvManager = object3dCommon_->GetSrvManager();
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle = srvManager->GetGPUDescriptorHandle(textureHandle_);
	commandList->SetGraphicsRootDescriptorTable(5, textureSrvHandle);

	D3D12_GPU_DESCRIPTOR_HANDLE environmentMapSrvHandle = srvManager->GetGPUDescriptorHandle(environmentMapHandle_);
	commandList->SetGraphicsRootDescriptorTable(6, environmentMapSrvHandle);
	if (object3dCommon_->IsShadowReady()) {
		commandList->SetGraphicsRootConstantBufferView(7, object3dCommon_->GetShadowSceneBufferAddress());
		commandList->SetGraphicsRootDescriptorTable(8, object3dCommon_->GetShadowMapSrvHandle());
	}

	if (useVertexShaderSkinning) {
		const Model::SkinCluster& skinCluster = model_->GetSkinCluster();
		if (skinCluster.paletteSrvHandle == Model::SkinCluster::kInvalidSrvHandle) {
			OutputDebugStringA("Skinned mesh has invalid MatrixPalette SRV handle.\n");
			assert(false && "Skinned mesh has invalid MatrixPalette SRV handle.");
			return;
		}
		commandList->SetGraphicsRootDescriptorTable(9, srvManager->GetGPUDescriptorHandle(skinCluster.paletteSrvHandle));
	}

	// モデルの描画実行
	object3dCommon_->GetDxCommon()->SetSceneRenderTargetsWithNormal();
	model_->Draw(object3dCommon_->GetDxCommon());
	object3dCommon_->GetDxCommon()->SetSceneRenderTarget();
}

void Object3d::DrawShadow() {
	if (!model_ || !object3dCommon_ || !object3dCommon_->IsShadowReady()) {
		return;
	}

	ID3D12GraphicsCommandList* commandList = object3dCommon_->GetDxCommon()->GetCommandList();
	const bool isSkinned = model_->HasSkinCluster();
	const bool useComputeSkinning = model_->UseComputeSkinning();
	const bool useVertexShaderSkinning = isSkinned && !useComputeSkinning;
	if (useComputeSkinning && !computeSkinningPrepared_) {
		model_->DispatchComputeSkinning(object3dCommon_);
		computeSkinningPrepared_ = true;
	}

	const Matrix4x4& matrixForRendering = isSkinned ? objectWorldMatrix_ : worldMatrix_;
	shadowTransformationMatrixData_->WVP = MatrixMath::Multiply(
		matrixForRendering, object3dCommon_->GetLightViewProjectionMatrix());
	shadowTransformationMatrixData_->World = matrixForRendering;

	commandList->SetGraphicsRootSignature(object3dCommon_->GetShadowRootSignature());
	commandList->SetPipelineState(object3dCommon_->GetShadowPipelineState(useVertexShaderSkinning));
	commandList->SetGraphicsRootConstantBufferView(
		0, shadowTransformationMatrixResource_->GetGPUVirtualAddress());

	if (useVertexShaderSkinning) {
		const Model::SkinCluster& skinCluster = model_->GetSkinCluster();
		if (skinCluster.paletteSrvHandle == Model::SkinCluster::kInvalidSrvHandle) {
			OutputDebugStringA("Shadow pass skipped: invalid MatrixPalette SRV handle.\n");
			assert(false && "Skinned shadow caster has invalid MatrixPalette SRV handle.");
			return;
		}
		commandList->SetGraphicsRootDescriptorTable(
			1, object3dCommon_->GetSrvManager()->GetGPUDescriptorHandle(skinCluster.paletteSrvHandle));
	}

	model_->DrawDepth(object3dCommon_->GetDxCommon());
}

void Object3d::SetModel(Model* model) {
	model_ = model;
	if (model_) {
		InitializeSkeleton();
	}
}

void Object3d::InitializeSkeleton() {
	if (model_) {
		skeleton_ = SkeletonSystem::CreateSkeleton(model_->GetRootNode());
	}
}
