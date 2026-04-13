#pragma once
#include "Object3dCommon.h"
#include "Model.h" 
#include "Matrix.h"
#include "Camera.h"
#include <wrl.h>
#include <d3d12.h>

class Object3d {
public:
	void Initialize(Object3dCommon* object3dCommon);
	void Update(Camera* camera);
	void Draw();
	void SetModel(Model* model);

	// 座標・回転・スケールの操作
	void SetPosition(const Vector3& position) { transform_.translate = position; }
	void SetRotation(const Vector3& rotation) { transform_.rotate = rotation; }
	void SetScale(const Vector3& scale) { transform_.scale = scale; }
	void SetTexture(uint32_t textureHandle) { textureHandle_ = textureHandle; }

	// ★追加：カリングとライティングの操作
	void SetCullMode(int cullMode) { cullMode_ = cullMode; }
	void SetShininess(float shininess) { materialData_->shininess = shininess; }
	float GetShininess() const { return materialData_->shininess; }

	void SetLightDirection(const Vector3& direction) { directionalLightData_->direction = direction; }
	void SetLightColor(const Vector4& color) { directionalLightData_->color = color; }
	void SetLightIntensity(float intensity) { directionalLightData_->intensity = intensity; }

private:
	Object3dCommon* object3dCommon_ = nullptr;
	Model* model_ = nullptr;
	uint32_t textureHandle_ = 0;
	int cullMode_ = 2; // Default: Backface

	struct Material {
		Vector4 color;
		int32_t enableLighting;
		float shininess;
		float padding[2];
		Matrix4x4 uvTransform;
	};

	struct TransformationMatrix {
		Matrix4x4 WVP;
		Matrix4x4 World;
	};

	struct DirectionalLight {
		Vector4 color;
		Vector3 direction;
		float intensity;
	};

	struct CameraForGPU {
		Vector3 worldPosition;
	};

	Transform transform_ = { {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };

	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
	Material* materialData_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResource_;
	TransformationMatrix* transformationMatrixData_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource_;
	DirectionalLight* directionalLightData_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> cameraResource_;
	CameraForGPU* cameraData_ = nullptr;
};