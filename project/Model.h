#pragma once
#include "DirectXCommon.h" 
#include "Matrix.h"
#include <string>
#include <vector>
#include <map>
#include <wrl.h>
#include <d3d12.h>

class ModelManager;
class SpriteCommon; // テクスチャ読み込み用

class Model {
public:
	// 初期化
	void Initialize(ModelManager* modelManager, const std::string& directoryPath, const std::string& filename);

	// ★追加: マテリアルに設定されたテクスチャを一括読み込みする関数
	void LoadTextures(SpriteCommon* spriteCommon);

	// 描画
	void Draw(DirectXCommon* dxCommon);

	// 特定のマテリアルのテクスチャパスを取得（代表として先頭のものなどを返す運用も可だが、基本はLoadTexturesにお任せ）
	const std::string& GetTextureFilePath() const { return materialData_.textureFilePath; }

private:
	void LoadObjFile(const std::string& directoryPath, const std::string& filename);
	void LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename);

	ModelManager* modelManager_ = nullptr;

	struct VertexData {
		Vector4 position;
		Vector2 texcoord;
		Vector3 normal;
	};

	// ★追加: マテリアルデータ（テクスチャパス ＋ ハンドル）
	struct ModelMaterialData {
		std::string textureFilePath;
		uint32_t textureHandle = 0;
	};

	// ★追加: メッシュデータ（頂点の範囲と使用するマテリアル名）
	struct Mesh {
		uint32_t start; // 開始頂点番号
		uint32_t count; // 頂点数
		std::string materialName; // このメッシュが使うマテリアル名
	};

	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
	std::vector<VertexData> vertices_;

	// 互換性のため残す（単一マテリアル用）
	struct MaterialData {
		std::string textureFilePath;
	};
	MaterialData materialData_;

	// ★追加: 複数のマテリアルとメッシュを管理するコンテナ
	std::map<std::string, ModelMaterialData> modelMaterials_;
	std::vector<Mesh> meshes_;
};