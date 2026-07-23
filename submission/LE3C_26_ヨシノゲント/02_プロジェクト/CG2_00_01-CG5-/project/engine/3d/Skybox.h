#pragma once
#include "base/DirectXCommon.h"
#include "2d/TextureManager.h"
#include "math/Matrix.h"
#include <wrl.h>
#include <d3d12.h>
#include <string>
#include <vector>

class Camera;

/**
 * Skyboxクラス
 * 資料に基づき、2x2x2の箱とDDSキューブマップの描画を管理します。
 */
class Skybox {
public:
	void Initialize(DirectXCommon* dxCommon, const std::string& ddsFilePath);
	void Update(Camera* camera);
	void Draw();

	// ★追加：環境マップ（映り込み）として利用するために、テクスチャの番号を取得する関数
	TextureCubeHandle GetTextureHandle() const { return textureHandle_; }

private:
	void CreateMesh(); // 資料スライド 11: 箱の作成
	void CreatePSO(); // 資料スライド 15: 専用PSOの作成

	DirectXCommon* dxCommon_ = nullptr;

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
	TextureCubeHandle textureHandle_{};

	// 描画設定
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
};
