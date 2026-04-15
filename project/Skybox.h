#pragma once
#include "DirectXCommon.h"
#include "Matrix.h"
#include <wrl.h>
#include <d3d12.h>
#include <string>
#include <vector>

class SrvManager;
class Camera;

/**
 * Skyboxクラス
 * 資料に基づき、2x2x2の箱とDDSキューブマップの描画を管理します。
 */
class Skybox {
public:
	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, const std::string& ddsFilePath);
	void Update(Camera* camera);
	void Draw();

private:
	void CreateMesh(); // 資料スライド 11: 箱の作成
	void LoadDDS(const std::string& filePath); // 資料スライド 9: DDSの読み込み
	void CreatePSO(); // 資料スライド 15: 専用PSOの作成

	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;

	// メッシュ情報
	Microsoft::WRL::ComPtr<ID3D12Resource> vertResource_;
	D3D12_VERTEX_BUFFER_VIEW vbView_{};
	Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;
	D3D12_INDEX_BUFFER_VIEW ibView_{};

	// 定数バッファ
	struct TransformationMatrix { Matrix4x4 WVP; Matrix4x4 World; };
	Microsoft::WRL::ComPtr<ID3D12Resource> constResource_;
	TransformationMatrix* constData_ = nullptr;

	struct Material { Vector4 color; };
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
	Material* materialData_ = nullptr;

	// テクスチャ
	Microsoft::WRL::ComPtr<ID3D12Resource> textureResource_;
	uint32_t srvIndex_ = 0;

	// 描画設定
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
};