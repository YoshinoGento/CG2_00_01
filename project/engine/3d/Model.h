#pragma once
#include "base/DirectXCommon.h" 
#include "math/Matrix.h"
#include <string>
#include <vector>
#include <map>
#include <wrl.h>
#include <d3d12.h>
#include <cstdint>
#include "3d/Animation.h"

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
		QuaternionTransform transform; // Transform情報
		Matrix4x4 localMatrix;
		std::string name;
		std::vector<Node> children;
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

	//　1頂点辺りに影響を与える骨の最大数
	static const uint32_t kMaxBoneInfluence = 4;

	//　1頂点ごとのスキンデータ
	struct VertexSkinning
	{
		float weights[kMaxBoneInfluence];      // 骨の影響度
		uint32_t boneIndices[kMaxBoneInfluence]; // 影響を受ける骨のインデックス
	};

	// モデルが保持する骨の情報
	struct Bone
	{
		std::string name;         //　骨の名前（Node名と一致）
		Matrix4x4 inverseBindMatrix; // Bind空間（初期姿勢）から骨空間への逆行列
	};


	void Initialize(ModelManager* modelManager, const std::string& directoryPath, const std::string& filename);
	void InitializeWithData(ModelManager* modelManager, const std::vector<VertexData>& vertices, const std::vector<uint32_t>& indices);
	void LoadTextures(SpriteCommon* spriteCommon);
	// 通常描画（1つだけ描画）
	void Draw(DirectXCommon* dxCommon);
	// インスタンシング描画（複数一括描画）
	void Draw(ID3D12GraphicsCommandList* commandList, uint32_t instanceCount);

	// --- 追加：ルートノードの取得 ---
	const Node& GetRootNode() const { return rootNode_; }

	// --- 追加：アニメーションの読み込み ---
	Animation LoadAnimation(const std::string& directoryPath, const std::string& filename);


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