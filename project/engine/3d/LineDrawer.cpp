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
	Microsoft::WRL::ComPtr<ID3DBlob> vsBlob;
	hr = D3DCompileFromFile(L"Resources/shader/LineVS.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "main", "vs_5_0", D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, &vsBlob, &errorBlob);
	if (FAILED(hr)) {
		if (errorBlob) {
			Logger::Log(std::string(reinterpret_cast<char*>(errorBlob->GetBufferPointer()), errorBlob->GetBufferSize()));
		}
		assert(false);
	}

	Microsoft::WRL::ComPtr<ID3DBlob> psBlob;
	hr = D3DCompileFromFile(L"Resources/shader/LinePS.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "main", "ps_5_0", D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, &psBlob, &errorBlob);
	if (FAILED(hr)) {
		if (errorBlob) {
			Logger::Log(std::string(reinterpret_cast<char*>(errorBlob->GetBufferPointer()), errorBlob->GetBufferSize()));
		}
		assert(false);
	}

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
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
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
