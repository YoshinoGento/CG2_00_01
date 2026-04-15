#pragma once
#include "DirectXCommon.h" 
#include "Matrix.h"
#include <string>
#include <vector>
#include <map>
#include <wrl.h>
#include <d3d12.h>
#include <cstdint>

// Assimpのヘッダー
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

class ModelManager;
class SpriteCommon;

/**
 * Modelクラス
 * glTF対応のため階層構造（Node）の保持機能を追加
 */
class Model {
public:
	// --- 追加：ノード構造体 ---
	struct Node {
		Matrix4x4 localMatrix;      // そのノード独自の変形行列
		std::string name;           // ノード名
		std::vector<Node> children; // 子供たち
	};

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

	// --- 追加：ルートノードの取得 ---
	const Node& GetRootNode() const { return rootNode_; }

private:
	// Assimp を使った読み込み関数
	void LoadModelFile(const std::string& directoryPath, const std::string& filename);

	// --- 追加：ノードを再帰的に読み込む関数 ---
	Node ReadNode(aiNode* node);

	ModelManager* modelManager_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
	std::vector<VertexData> vertices_;

	// インデックスバッファ
	Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;
	D3D12_INDEX_BUFFER_VIEW indexBufferView_{};
	std::vector<uint32_t> indices_;
	UINT indexCount_ = 0;

	struct ModelMaterialData {
		std::string textureFilePath;
		uint32_t textureHandle = 0;
	};
	std::map<std::string, ModelMaterialData> modelMaterials_;
	std::vector<Mesh> meshes_;

	// --- 追加：このモデルの根本（ルート）となるノード ---
	Node rootNode_;
};