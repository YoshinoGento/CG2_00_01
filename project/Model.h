#pragma once
#include "DirectXCommon.h" 
#include "Matrix.h"
#include <string>
#include <vector>
#include <map>
#include <wrl.h>
#include <d3d12.h>

class ModelManager;
class SpriteCommon;

class Model {
public:
	// ★修正: 構造体の定義を最初（public）に移動
	// 鏡面反射の実装や PrimitiveGenerator での初期化に合わせ、メンバの順序を
	// 「座標 (Vector4) -> 法線 (Vector3) -> UV (Vector2)」の順に入れ替えました。
	struct VertexData {
		Vector4 position;
		Vector3 normal;
		Vector2 texcoord;
	};

	// ★修正: メッシュデータの定義も public へ
	struct Mesh {
		uint32_t start; // 開始頂点番号
		uint32_t count; // 頂点数
		std::string materialName; // このメッシュが使うマテリアル名
	};

	// ファイルから初期化
	void Initialize(ModelManager* modelManager, const std::string& directoryPath, const std::string& filename);

	// ★追加: メモリ上のデータから初期化（プリミティブ生成用）
	void InitializeWithData(ModelManager* modelManager, const std::vector<VertexData>& vertices, const std::vector<uint32_t>& indices);

	// マテリアルに設定されたテクスチャを一括読み込み
	void LoadTextures(SpriteCommon* spriteCommon);

	// 描画
	void Draw(DirectXCommon* dxCommon);

	const std::string& GetTextureFilePath() const { return materialData_.textureFilePath; }

private:
	void LoadObjFile(const std::string& directoryPath, const std::string& filename);
	void LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename);

	ModelManager* modelManager_ = nullptr;

	// 頂点バッファ関連
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
	std::vector<VertexData> vertices_;

	// ★追加: インデックスバッファ関連（球体などの描画に使用）
	Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;
	D3D12_INDEX_BUFFER_VIEW indexBufferView_{};
	UINT indexCount_ = 0;

	// マテリアルデータ
	struct ModelMaterialData {
		std::string textureFilePath;
		uint32_t textureHandle = 0;
	};

	struct MaterialData {
		std::string textureFilePath;
	};
	MaterialData materialData_;

	std::map<std::string, ModelMaterialData> modelMaterials_;
	std::vector<Mesh> meshes_;
};