#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include "math/Struct.h"
#include "math/Matrix.h"

class DirectXCommon;

/**
 * LineDrawer
 * デバッグ用などに画面に線を引く機能を提供するクラス
 */
class LineDrawer {
public:
	struct LineVertex {
		Vector3 position;
		Vector4 color;
	};

	struct TransformData {
		Matrix4x4 WVP;
		Matrix4x4 World;
	};

	static LineDrawer* GetInstance();

	// 初期化
	void Initialize(DirectXCommon* dxCommon);

	// 線を追加（毎フレーム呼び出し、Draw時にまとめて描画）
	void DrawLine(const Vector3& start, const Vector3& end, const Vector4& color = { 1.0f, 1.0f, 1.0f, 1.0f });

	// 溜まった線を描画してクリア
	void Draw(const Matrix4x4& viewProjectionMatrix);

private:
	LineDrawer() = default;
	~LineDrawer() = default;
	LineDrawer(const LineDrawer&) = delete;
	LineDrawer& operator=(const LineDrawer&) = delete;

	DirectXCommon* dxCommon_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;

	static const uint32_t MAX_VERTICES = 10000;
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
	LineVertex* vertexData_ = nullptr;
	uint32_t currentVertexCount_ = 0;

	Microsoft::WRL::ComPtr<ID3D12Resource> transformResource_;
	TransformData* transformData_ = nullptr;

	void CreatePipeline();
};
