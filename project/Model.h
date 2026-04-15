#pragma once
#include "DirectXCommon.h" 
#include "Matrix.h"
#include <string>
#include <vector>
#include <map>
#include <wrl.h>
#include <d3d12.h>

// Assimpのヘッダー
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

class ModelManager;
class SpriteCommon;

class Model {
public:
	struct VertexData {
		Vector4 position;
		Vector3 normal;
		Vector2 texcoord;
	};

	struct Mesh {
		uint32_t start;
		uint32_t count;
		std::string materialName;
	};

	void Initialize(ModelManager* modelManager, const std::string& directoryPath, const std::string& filename);
	void InitializeWithData(ModelManager* modelManager, const std::vector<VertexData>& vertices, const std::vector<uint32_t>& indices);
	void LoadTextures(SpriteCommon* spriteCommon);
	void Draw(DirectXCommon* dxCommon);

private:
	// Assimp を使った新しい読み込み関数
	void LoadModelFile(const std::string& directoryPath, const std::string& filename);

	ModelManager* modelManager_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
	std::vector<VertexData> vertices_;

	// インデックスバッファ
	Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;
	D3D12_INDEX_BUFFER_VIEW indexBufferView_{};
	std::vector<uint32_t> indices_; // インデックスデータ
	UINT indexCount_ = 0;

	struct ModelMaterialData {
		std::string textureFilePath;
		uint32_t textureHandle = 0;
	};
	std::map<std::string, ModelMaterialData> modelMaterials_;
	std::vector<Mesh> meshes_;
};