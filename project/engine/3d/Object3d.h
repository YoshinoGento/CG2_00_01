#pragma once
#include "3d/Object3dCommon.h"
#include "3d/Model.h" 
#include "math/Matrix.h"
#include "3d/Camera.h"
#include "3d/Skeleton.h"
#include "2d/TextureManager.h"
#include <cstdint>
#include <optional>
#include <wrl.h>
#include <d3d12.h>


/**
 * Object3dクラス
 * スポットライトの管理機能を追加
 */
class Object3d {
public:
	enum class SpecularType : int32_t {
		Phong = 0,
		BlinnPhong = 1,
	};

	// シェーダーと一致させる構造体 (16バイト境界に注意)
	void Initialize(Object3dCommon* object3dCommon);
	void Update(Camera* camera, float deltaTime);
	void Draw();
	void DrawShadow();
	void SetModel(Model* model);
	void InitializeSkeleton();

	std::optional<Skeleton>& GetSkeleton() { return skeleton_; }
	Model* GetModel() const { return model_; }
	const Matrix4x4& GetWorldMatrix() const { return worldMatrix_; }
	// ルートノードのローカル行列を含まない、オブジェクト自身の純粋なアフィン変換行列を取得する（日本語コメント付き）
	const Matrix4x4& GetObjectWorldMatrix() const { return objectWorldMatrix_; }
	Vector3 GetPosition() const { return transform_.translate; }
	Vector3 GetRotation() const { return transform_.rotate; }
	Vector3 GetScale() const { return transform_.scale; }

	void SetPosition(const Vector3& position) { transform_.translate = position; }
	void SetRotation(const Vector3& rotation) { transform_.rotate = rotation; }
	bool SetScale(const Vector3& scale);
	void SetTexture(Texture2DHandle textureHandle) { textureHandle_ = textureHandle; }
	void SetColor(const Vector4& color) { materialData_->color = color; }
	void SetEnableLighting(bool enabled) { materialData_->enableLighting = enabled ? 1 : 0; }
	// 環境マップ用のテクスチャハンドルをセット
	void SetEnvironmentMap(TextureCubeHandle handle) { environmentMapHandle_ = handle; }
	// 反射強度のセット
	void SetEnvironmentCoefficient(float coef) { materialData_->environmentCoefficient = coef; }
	void SetCullMode(int cullMode);
	void SetShininess(float shininess) { materialData_->shininess = shininess; }
	void SetSpecularType(SpecularType type) { materialData_->specularType = static_cast<int32_t>(type); }


	// 平行光源の設定

	// ★追加：スポットライトの設定用Setter

	// アニメーション用アクセサ
	void SetAnimation(const Animation& animation) { animation_ = animation; }
	float& GetAnimationTime() { return animationTime_; }
	bool& GetIsAnimationPlaying() { return isAnimationPlaying_; }
	float& GetAnimationSpeed() { return animationSpeed_; }
	const Animation& GetAnimation() const { return animation_; }
	bool HasAnimation() const { return animation_.duration > 0.0f; }
	bool HasSkeleton() const { return skeleton_.has_value(); }
	bool HasSkinning() const { return model_ && model_->HasSkinCluster(); }
	bool IsAnimationPlaying() const { return isAnimationPlaying_; }
	void SetAnimationPlaying(bool isPlaying) { isAnimationPlaying_ = isPlaying; }
	void ResetAnimationTime() { animationTime_ = 0.0f; }
	float GetAnimationDuration() const { return animation_.duration; }
	float GetAnimationTimeValue() const { return animationTime_; }
	float GetPlaybackSpeed() const { return animationSpeed_; }
	void SetPlaybackSpeed(float speed) { animationSpeed_ = speed; }


private:
	Object3dCommon* object3dCommon_ = nullptr;
	Model* model_ = nullptr;
	Texture2DHandle textureHandle_{};
	TextureCubeHandle environmentMapHandle_{};
	int cullMode_ = 2;
	Animation animation_;
	float animationTime_ = 0.0f;
	bool isAnimationPlaying_ = true;
	float animationSpeed_ = 1.0f;
	std::optional<Skeleton> skeleton_;
	Matrix4x4 worldMatrix_ = MatrixMath::MakeIdentity4x4();
	// ：ルートノードのローカル行列を含まない、オブジェクト自身のワールド変換行列
	Matrix4x4 objectWorldMatrix_ = MatrixMath::MakeIdentity4x4();
	bool computeSkinningPrepared_ = false;
	bool isMirrored_ = false;

	struct Material {
		Vector4 color;
		int32_t enableLighting;
		float shininess;
		float environmentCoefficient;
		int32_t specularType;
		Matrix4x4 uvTransform;
	};
	static_assert(sizeof(Material) == 96);

	// GPUに送るための頂点スキンデータ
	struct VertexShaderSkinning
	{
		Vector4 weights;     //頂点シェイダー側でfloat4となる重み
		int32_t boneIndices[4]; //頂点シェイダー側でint4となる骨のインデックス
	};

	struct TransformationMatrix {
		Matrix4x4 WVP;
		Matrix4x4 World;
		Matrix4x4 WorldInverseTranspose;
	};
	static_assert(sizeof(TransformationMatrix) == sizeof(Matrix4x4) * 3);

	Transform transform_ = { {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };

	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
	Material* materialData_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResource_;
	TransformationMatrix* transformationMatrixData_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> shadowTransformationMatrixResource_;
	TransformationMatrix* shadowTransformationMatrixData_ = nullptr;

	// スポットライト用

	public:
		// 修正：構造体の定義より後ろのこの位置に配置することで、コンパイルが正常に通ります
		Material* GetMaterialData() { return materialData_; }
		TransformationMatrix* GetTransformationMatrixData() { return transformationMatrixData_; }

		// アニメーションによる骨の上書きを制御するためのフラグ操作（オプション）
		bool isBoneManualControl_ = false;
};
