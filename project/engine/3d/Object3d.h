#pragma once
#include "Object3dCommon.h"
#include "Model.h" 
#include "Matrix.h"
#include "Camera.h"
#include <wrl.h>
#include <d3d12.h>

/**
 * Object3dクラス
 * スポットライトの管理機能を追加
 */
class Object3d {
public:
	// シェーダーと一致させる構造体 (16バイト境界に注意)
	struct SpotLightData {
		Vector4 color;
		Vector3 position;
		float intensity;
		Vector3 direction;
		float distance;
		float decay;
		float cosAngle;
		float cosFalloffStart;
		float padding[2];
	};

	void Initialize(Object3dCommon* object3dCommon);
	void Update(Camera* camera);
	void Draw();
	void SetModel(Model* model);

	void SetPosition(const Vector3& position) { transform_.translate = position; }
	void SetRotation(const Vector3& rotation) { transform_.rotate = rotation; }
	void SetScale(const Vector3& scale) { transform_.scale = scale; }
	void SetTexture(uint32_t textureHandle) { textureHandle_ = textureHandle; }
	void SetCullMode(int cullMode) { cullMode_ = cullMode; }
	void SetShininess(float shininess) { materialData_->shininess = shininess; }

	// 平行光源の設定
	void SetLightDirection(const Vector3& direction) { directionalLightData_->direction = direction; }
	void SetLightColor(const Vector4& color) { directionalLightData_->color = color; }
	void SetLightIntensity(float intensity) { directionalLightData_->intensity = intensity; }

	// ★追加：スポットライトの設定用Setter
	void SetSpotLightColor(const Vector4& color) { spotLightData_->color = color; }
	void SetSpotLightPosition(const Vector3& pos) { spotLightData_->position = pos; }
	void SetSpotLightDirection(const Vector3& dir) { spotLightData_->direction = dir; }
	void SetSpotLightDistance(float dist) { spotLightData_->distance = dist; }
	void SetSpotLightIntensity(float intensity) { spotLightData_->intensity = intensity; }
	void SetSpotLightDecay(float decay) { spotLightData_->decay = decay; }
	void SetSpotLightAngle(float angleRad) { spotLightData_->cosAngle = std::cos(angleRad); }
	void SetSpotLightFalloff(float angleRad) { spotLightData_->cosFalloffStart = std::cos(angleRad); }

private:
	Object3dCommon* object3dCommon_ = nullptr;
	Model* model_ = nullptr;
	uint32_t textureHandle_ = 0;
	int cullMode_ = 2;

	struct Material { Vector4 color; int32_t enableLighting; float shininess; float padding[2]; Matrix4x4 uvTransform; };
	struct TransformationMatrix { Matrix4x4 WVP; Matrix4x4 World; };
	struct DirectionalLight { Vector4 color; Vector3 direction; float intensity; };
	struct CameraForGPU { Vector3 worldPosition; };

	Transform transform_ = { {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };

	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
	Material* materialData_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResource_;
	TransformationMatrix* transformationMatrixData_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource_;
	DirectionalLight* directionalLightData_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> cameraResource_;
	CameraForGPU* cameraData_ = nullptr;

	// ★追加：スポットライト用
	Microsoft::WRL::ComPtr<ID3D12Resource> spotLightResource_;
	SpotLightData* spotLightData_ = nullptr;
};