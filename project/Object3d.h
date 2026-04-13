#pragma once
#include "Object3dCommon.h"
#include "Model.h" 
#include "Matrix.h"
#include "Camera.h"

class Object3d {
public:
	void Initialize(Object3dCommon* object3dCommon);
	void Update(Camera* camera);
	void Draw();
	void SetModel(Model* model);

	void SetPosition(const Vector3& position) { transform_.translate = position; }
	void SetRotation(const Vector3& rotation) { transform_.rotate = rotation; }
	void SetScale(const Vector3& scale) { transform_.scale = scale; }
	void SetTexture(uint32_t textureHandle) { textureHandle_ = textureHandle; }

	// ★追加: 鏡面反射の強さを設定
	void SetShininess(float shininess) { materialData_->shininess = shininess; }
	float GetShininess() const { return materialData_->shininess; }

private:
	Object3dCommon* object3dCommon_ = nullptr;
	Model* model_ = nullptr;
	uint32_t textureHandle_ = 0;

	struct Material {
		Vector4 color;
		int32_t enableLighting;
		float shininess; // ★追加
		float padding[2]; // パディング調整
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

	// ★追加: GPUに送るカメラ用構造体
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

	// ★追加: カメラリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> cameraResource_;
	CameraForGPU* cameraData_ = nullptr;
};