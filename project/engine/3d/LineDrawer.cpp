#include "3d/LineDrawer.h"
#include "base/DirectXCommon.h"
#include "base/Logger.h"
#include "base/Logger.h"
#include <cassert>
#include <d3dcompiler.h>

#pragma comment(lib, "d3dcompiler.lib")

LineDrawer* LineDrawer::GetInstance() {
	static LineDrawer instance;
	return &instance;
}

void LineDrawer::Initialize(DirectXCommon* dxCommon) {
	dxCommon_ = dxCommon;

	// 頂点バッファ作成
	vertexResource_ = dxCommon_->CreateBufferResource(sizeof(LineVertex) * MAX_VERTICES);
	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
	vertexBufferView_.SizeInBytes = sizeof(LineVertex) * MAX_VERTICES;
	vertexBufferView_.StrideInBytes = sizeof(LineVertex);
	vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));

	// 定数バッファ作成
	transformResource_ = dxCommon_->CreateBufferResource(sizeof(TransformData));
	transformResource_->Map(0, nullptr, reinterpret_cast<void**>(&transformData_));
	transformData_->World = MatrixMath::MakeIdentity4x4();
	transformData_->WVP = MatrixMath::MakeIdentity4x4();

	CreatePipeline();
}

void LineDrawer::CreatePipeline() {
	HRESULT hr;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

	// シェーダーコンパイル
	Microsoft::WRL::ComPtr<IDxcBlob> vsBlob = dxCommon_->CompileShader(L"Resources/shader/LineVS.hlsl", L"vs_6_0");
	Microsoft::WRL::ComPtr<IDxcBlob> psBlob = dxCommon_->CompileShader(L"Resources/shader/LinePS.hlsl", L"ps_6_0");

	// ルートシグネチャ作成
	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	D3D12_ROOT_PARAMETER rootParameters[1] = {};
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[0].Descriptor.ShaderRegister = 0;

	rootSignatureDesc.pParameters = rootParameters;
	rootSignatureDesc.NumParameters = 1;

	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
	hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr)) {
		Logger::Log(std::string(reinterpret_cast<char*>(errorBlob->GetBufferPointer()), errorBlob->GetBufferSize()));
		assert(false);
	}

	hr = dxCommon_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
	assert(SUCCEEDED(hr));

	// インプットレイアウト
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	// グラフィックスパイプライン
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
	psoDesc.pRootSignature = rootSignature_.Get();
	psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
	psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
	psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };
	
	D3D12_BLEND_DESC blendDesc{};
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	psoDesc.BlendState = blendDesc;

	D3D12_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
	rasterizerDesc.DepthClipEnable = TRUE;
	psoDesc.RasterizerState = rasterizerDesc;

	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	depthStencilDesc.DepthEnable = TRUE;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	psoDesc.DepthStencilState = depthStencilDesc;
	psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.SampleDesc.Count = 1;

	hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState_));
	assert(SUCCEEDED(hr));
}

void LineDrawer::DrawLine(const Vector3& start, const Vector3& end, const Vector4& color) {
	if (currentVertexCount_ + 2 > MAX_VERTICES) return;

	vertexData_[currentVertexCount_] = { start, color };
	vertexData_[currentVertexCount_ + 1] = { end, color };
	currentVertexCount_ += 2;
}

void LineDrawer::Draw(const Matrix4x4& viewProjectionMatrix) {
	if (currentVertexCount_ == 0) return;

	transformData_->WVP = viewProjectionMatrix;

	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
	commandList->SetPipelineState(pipelineState_.Get());
	commandList->SetGraphicsRootSignature(rootSignature_.Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
	commandList->SetGraphicsRootConstantBufferView(0, transformResource_->GetGPUVirtualAddress());

	commandList->DrawInstanced(currentVertexCount_, 1, 0, 0);

	// 描画後はクリア
	currentVertexCount_ = 0;
}

void LineDrawer::DrawWireCube(const Vector3& center, float size, const Vector4& color) {
	float halfSize = size * 0.5f;
	Vector3 v[8] = {
		{center.x - halfSize, center.y - halfSize, center.z - halfSize},
		{center.x + halfSize, center.y - halfSize, center.z - halfSize},
		{center.x + halfSize, center.y + halfSize, center.z - halfSize},
		{center.x - halfSize, center.y + halfSize, center.z - halfSize},
		{center.x - halfSize, center.y - halfSize, center.z + halfSize},
		{center.x + halfSize, center.y - halfSize, center.z + halfSize},
		{center.x + halfSize, center.y + halfSize, center.z + halfSize},
		{center.x - halfSize, center.y + halfSize, center.z + halfSize}
	};
	// Front face
	DrawLine(v[0], v[1], color); DrawLine(v[1], v[2], color); DrawLine(v[2], v[3], color); DrawLine(v[3], v[0], color);
	// Back face
	DrawLine(v[4], v[5], color); DrawLine(v[5], v[6], color); DrawLine(v[6], v[7], color); DrawLine(v[7], v[4], color);
	// Connecting edges
	DrawLine(v[0], v[4], color); DrawLine(v[1], v[5], color); DrawLine(v[2], v[6], color); DrawLine(v[3], v[7], color);
}

void LineDrawer::DrawWireSphere(const Vector3& center, float radius, const Vector4& color, uint32_t segments) {
	const float pi = 3.14159265358979323846f;

	// 水平方向（緯度）のリング
	for (uint32_t lat = 1; lat < segments; ++lat) {
		float phi = pi * lat / segments;
		float y = std::cos(phi);
		float r = std::sin(phi);

		for (uint32_t lon = 0; lon < segments; ++lon) {
			float theta1 = 2.0f * pi * lon / segments;
			float theta2 = 2.0f * pi * (lon + 1) / segments;

			Vector3 p1 = { center.x + radius * r * std::cos(theta1), center.y + radius * y, center.z + radius * r * std::sin(theta1) };
			Vector3 p2 = { center.x + radius * r * std::cos(theta2), center.y + radius * y, center.z + radius * r * std::sin(theta2) };
			DrawLine(p1, p2, color);
		}
	}

	// 垂直方向（経度）の半円
	for (uint32_t lon = 0; lon < segments; ++lon) {
		float theta = 2.0f * pi * lon / segments;
		float cx = std::cos(theta);
		float cz = std::sin(theta);

		for (uint32_t lat = 0; lat < segments; ++lat) {
			float phi1 = pi * lat / segments;
			float phi2 = pi * (lat + 1) / segments;

			Vector3 p1 = { center.x + radius * std::sin(phi1) * cx, center.y + radius * std::cos(phi1), center.z + radius * std::sin(phi1) * cz };
			Vector3 p2 = { center.x + radius * std::sin(phi2) * cx, center.y + radius * std::cos(phi2), center.z + radius * std::sin(phi2) * cz };
			DrawLine(p1, p2, color);
		}
	}
}

void LineDrawer::DrawWireTriangle(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector4& color) {
	DrawLine(p0, p1, color);
	DrawLine(p1, p2, color);
	DrawLine(p2, p0, color);
}
