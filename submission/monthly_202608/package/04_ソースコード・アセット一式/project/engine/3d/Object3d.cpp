#include "3d/Object3d.h"
#include "3d/Object3dCommon.h"
#include "3d/LightingSystem.h"
#include "base/SrvManager.h"
#include "base/DirectXCommon.h"
#include "2d/TextureManager.h"
#include "base/Logger.h"
#include "base/FrameClock.h"
#include <algorithm>
#include <cassert>
#include <cmath>

namespace {
	constexpr float kMinimumScaleMagnitude = 1.0e-4f;

	float Determinant3x3(const Matrix4x4& matrix) {
		return
			matrix.m[0][0] * (matrix.m[1][1] * matrix.m[2][2] - matrix.m[1][2] * matrix.m[2][1]) -
			matrix.m[0][1] * (matrix.m[1][0] * matrix.m[2][2] - matrix.m[1][2] * matrix.m[2][0]) +
			matrix.m[0][2] * (matrix.m[1][0] * matrix.m[2][1] - matrix.m[1][1] * matrix.m[2][0]);
	}

	int ResolveCullMode(int requestedCullMode, bool isMirrored) {
		if (!isMirrored || requestedCullMode == 0) {
			return requestedCullMode;
		}
		return requestedCullMode == 1 ? 2 : 1;
	}
}

void Object3d::Initialize(Object3dCommon* object3dCommon) {
	assert(object3dCommon);
	object3dCommon_ = object3dCommon;
	textureHandle_ = TextureManager::GetInstance()->GetFallback2D();
	environmentMapHandle_ = TextureManager::GetInstance()->GetFallbackCube();
	DirectXCommon* dxCommon = object3dCommon_->GetDxCommon();

	materialResource_ = dxCommon->CreateBufferResource(sizeof(Material));
	materialResource_->Map(0, nullptr, (void**)&materialData_);
	materialData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	materialData_->enableLighting = 1;
	materialData_->shininess = 40.0f;
	materialData_->environmentCoefficient = 0.0f;
	materialData_->specularType = static_cast<int32_t>(Object3d::SpecularType::BlinnPhong);
	materialData_->uvTransform = MatrixMath::MakeIdentity4x4();

	transformationMatrixResource_ = dxCommon->CreateBufferResource(sizeof(TransformationMatrix));
	transformationMatrixResource_->Map(0, nullptr, (void**)&transformationMatrixData_);
	shadowTransformationMatrixResource_ = dxCommon->CreateBufferResource(sizeof(TransformationMatrix));
	shadowTransformationMatrixResource_->Map(0, nullptr, (void**)&shadowTransformationMatrixData_);
	const Matrix4x4 identity = MatrixMath::MakeIdentity4x4();
	*transformationMatrixData_ = { identity, identity, identity };
	*shadowTransformationMatrixData_ = { identity, identity, identity };

}

void Object3d::Update(Camera* camera) {
	Update(camera, 0.0f);
}

void Object3d::Update(Camera* camera, float deltaTime) {
	assert(camera);
	computeSkinningPrepared_ = false;
	if (!std::isfinite(deltaTime)) {
		deltaTime = 0.0f;
	}
	deltaTime = std::clamp(deltaTime, 0.0f, FrameClock::kMaximumFrameDeltaSeconds);

	if (isAnimationPlaying_ && animation_.duration > 0.0f) {
		animationTime_ += deltaTime * animationSpeed_;
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
	const float worldDeterminant = Determinant3x3(matrixForRendering);
	transformationMatrixData_->WorldInverseTranspose = std::abs(worldDeterminant) >= 1.0e-8f
		? MatrixMath::Transpose(MatrixMath::Inverse(matrixForRendering))
		: MatrixMath::MakeIdentity4x4();
	isMirrored_ = worldDeterminant < 0.0f;

	// カメラ座標の更新

	// --- 4. スポットライトの更新 (正規化の追加) ---
	// ImGuiなどで方向が変更された場合、計算に使う前に必ず長さを 1 にします
	// これを行わないと、シェーダー側で角度による減衰計算が破綻します
}

void Object3d::SetTexture(uint32_t textureHandle) {
	textureHandle_ = TextureManager::GetInstance()->GetTexture2DHandle(textureHandle);
}

void Object3d::Draw() {
	if (!model_) return;
	if (!object3dCommon_->IsObjectPassActive()) {
		Logger::Log("Object3d::Draw rejected because BeginObjectPass was not called.");
		assert(false && "Object3d draw outside object pass");
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

	// 3Dオブジェクト用のルートシグネチャを明示的にセットする
	commandList->SetGraphicsRootSignature(object3dCommon_->GetRootSignature(useVertexShaderSkinning));

	// パイプラインをセット
	commandList->SetPipelineState(object3dCommon_->GetPipelineState(
		ResolveCullMode(cullMode_, isMirrored_), useVertexShaderSkinning));

	// RootSignature の Index に合わせて定数バッファをセット
	commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(1, transformationMatrixResource_->GetGPUVirtualAddress());
	LightingSystem* lightingSystem = object3dCommon_->GetLightingSystem();
	assert(lightingSystem != nullptr);
	commandList->SetGraphicsRootConstantBufferView(2, lightingSystem->GetDirectionalLightAddress());
	commandList->SetGraphicsRootConstantBufferView(3, lightingSystem->GetCameraAddress());
	commandList->SetGraphicsRootConstantBufferView(4, lightingSystem->GetSpotLightAddress());

	SrvManager* srvManager = object3dCommon_->GetSrvManager();
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle = TextureManager::GetInstance()->GetGpuHandle(textureHandle_);
	commandList->SetGraphicsRootDescriptorTable(5, textureSrvHandle);

	D3D12_GPU_DESCRIPTOR_HANDLE environmentMapSrvHandle = TextureManager::GetInstance()->GetGpuHandle(environmentMapHandle_);
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
	model_->Draw(object3dCommon_->GetDxCommon());
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
	const float shadowWorldDeterminant = Determinant3x3(matrixForRendering);
	shadowTransformationMatrixData_->WorldInverseTranspose = std::abs(shadowWorldDeterminant) >= 1.0e-8f
		? MatrixMath::Transpose(MatrixMath::Inverse(matrixForRendering))
		: MatrixMath::MakeIdentity4x4();

	commandList->SetGraphicsRootSignature(object3dCommon_->GetShadowRootSignature());
	commandList->SetPipelineState(object3dCommon_->GetShadowPipelineState(useVertexShaderSkinning, isMirrored_));
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

bool Object3d::SetScale(const Vector3& scale) {
	if (std::abs(scale.x) < kMinimumScaleMagnitude ||
		std::abs(scale.y) < kMinimumScaleMagnitude ||
		std::abs(scale.z) < kMinimumScaleMagnitude) {
		Logger::Log("Object3d::SetScale rejected a near-zero component because its normal matrix would be singular.");
		return false;
	}
	transform_.scale = scale;
	return true;
}

void Object3d::SetCullMode(int cullMode) {
	cullMode_ = std::clamp(cullMode, 0, 2);
}

void Object3d::InitializeSkeleton() {
	if (model_) {
		skeleton_ = SkeletonSystem::CreateSkeleton(model_->GetRootNode());
	}
}
