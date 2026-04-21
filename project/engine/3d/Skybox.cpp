#include "3d/Skybox.h"
#include "base/SrvManager.h"
#include "3d/Camera.h"
#include <cassert>

using namespace Microsoft::WRL;

/**
 * 初期化
 */
void Skybox::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, const std::string& ddsFilePath) {
	dxCommon_ = dxCommon;
	srvManager_ = srvManager;

	// 1. 頂点データ（-1.0 ~ 1.0 の 2m四方）を作成
	CreateMesh();

	// 2. ロストック・ラーゲ空港の4Kパノラマデータをロード
	LoadDDS(ddsFilePath);

	// 3. 定数バッファ作成
	constResource_ = dxCommon_->CreateBufferResource(sizeof(TransformationMatrix));
	constResource_->Map(0, nullptr, (void**)&constData_);

	materialResource_ = dxCommon_->CreateBufferResource(sizeof(Material));
	materialResource_->Map(0, nullptr, (void**)&materialData_);
	materialData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };

	// 4. スライド15ページの指定通り、深度書き込み無効のPSOを作成
	CreatePSO();
}

/**
 * 更新
 */
void Skybox::Update(Camera* camera) {
	// カメラの座標を取得
	Vector3 camPos = camera->GetTranslate();

	// ★スライドの指示を反映：WorldMatrix を使ってスケーリングを行う
	// 2mの箱を大きな値（例：500m）でスケーリングして、シーン全体を包み込むようにします
	// z=wトリックを併用しているため、この値がいくつであっても常に「最背面」に描画されます
	float scale = 500.0f;
	Matrix4x4 worldMatrix = MatrixMath::MakeAffineMatrix(
		{ scale, scale, scale }, // Scale: ここで適切に大きくする
		{ 0.0f, 0.0f, 0.0f },    // Rotate
		camPos                   // Translate: カメラに追従させる
	);

	constData_->World = worldMatrix;
	constData_->WVP = MatrixMath::Multiply(worldMatrix, camera->GetViewProjectionMatrix());
}

/**
 * 描画
 */
void Skybox::Draw() {
	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

	commandList->SetPipelineState(pipelineState_.Get());
	commandList->SetGraphicsRootSignature(rootSignature_.Get());

	// ★重要：トポロジー（頂点の繋がり方）をセット
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	commandList->IASetVertexBuffers(0, 1, &vbView_);
	commandList->IASetIndexBuffer(&ibView_);

	commandList->SetGraphicsRootConstantBufferView(0, constResource_->GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(1, materialResource_->GetGPUVirtualAddress());

	D3D12_GPU_DESCRIPTOR_HANDLE handle = srvManager_->GetGPUDescriptorHandle(srvIndex_);
	commandList->SetGraphicsRootDescriptorTable(2, handle);

	// 36頂点を描画
	commandList->DrawIndexedInstanced(36, 1, 0, 0, 0);
}

/**
 * メッシュ作成（-1.0 ~ 1.0）
 */
void Skybox::CreateMesh() {
	struct Vertex { Vector4 pos; };
	Vertex vertices[] = {
		{{-1, 1,-1, 1}}, {{ 1, 1,-1, 1}}, {{-1,-1,-1, 1}}, {{ 1,-1,-1, 1}}, // 前
		{{-1, 1, 1, 1}}, {{ 1, 1, 1, 1}}, {{-1,-1, 1, 1}}, {{ 1,-1, 1, 1}}, // 後
	};
	uint32_t indices[] = {
		0, 1, 2, 2, 1, 3, // 前
		4, 6, 5, 5, 6, 7, // 後（内側を向かせるため逆順）
		4, 5, 0, 0, 5, 1, // 上
		2, 3, 6, 6, 3, 7, // 下
		4, 0, 6, 6, 0, 2, // 左
		1, 5, 3, 3, 5, 7  // 右
	};

	vertResource_ = dxCommon_->CreateBufferResource(sizeof(vertices));
	void* vData; vertResource_->Map(0, nullptr, &vData); memcpy(vData, vertices, sizeof(vertices));
	vbView_.BufferLocation = vertResource_->GetGPUVirtualAddress();
	vbView_.SizeInBytes = sizeof(vertices); vbView_.StrideInBytes = sizeof(Vertex);

	indexResource_ = dxCommon_->CreateBufferResource(sizeof(indices));
	void* iData; indexResource_->Map(0, nullptr, &iData); memcpy(iData, indices, sizeof(indices));
	ibView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
	ibView_.SizeInBytes = sizeof(indices); ibView_.Format = DXGI_FORMAT_R32_UINT;
}

/**
 * DDS(TextureCube)のロード
 */
void Skybox::LoadDDS(const std::string& filePath) {
	DirectX::ScratchImage image;
	std::wstring wpath(filePath.begin(), filePath.end());
	HRESULT hr = DirectX::LoadFromDDSFile(wpath.c_str(), DirectX::DDS_FLAGS_NONE, nullptr, image);
	assert(SUCCEEDED(hr));

	const DirectX::TexMetadata& metadata = image.GetMetadata();
	textureResource_ = dxCommon_->CreateTextureResource(metadata);
	dxCommon_->UploadTextureData(textureResource_.Get(), image);

	srvIndex_ = srvManager_->Allocate();
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = metadata.format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE; // 立体テクスチャとして設定
	srvDesc.TextureCube.MostDetailedMip = 0;
	srvDesc.TextureCube.MipLevels = (UINT)metadata.mipLevels;
	srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;

	dxCommon_->GetDevice()->CreateShaderResourceView(textureResource_.Get(), &srvDesc, srvManager_->GetCPUDescriptorHandle(srvIndex_));
}

/**
 * PSO作成
 */
void Skybox::CreatePSO() {
	ID3D12Device* device = dxCommon_->GetDevice();
	auto vs = dxCommon_->CompileShader(L"Resources/shader/Skybox.VS.hlsl", L"vs_6_0");
	auto ps = dxCommon_->CompileShader(L"Resources/shader/Skybox.PS.hlsl", L"ps_6_0");

	D3D12_ROOT_PARAMETER rootParams[3] = {};
	rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParams[0].Descriptor.ShaderRegister = 0; // b0
	rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParams[1].Descriptor.ShaderRegister = 1; // b1

	D3D12_DESCRIPTOR_RANGE range = {};
	range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	range.NumDescriptors = 1;
	range.BaseShaderRegister = 0; // t0
	rootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParams[2].DescriptorTable.pDescriptorRanges = &range;
	rootParams[2].DescriptorTable.NumDescriptorRanges = 1;

	D3D12_STATIC_SAMPLER_DESC sampler = {};
	sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_ROOT_SIGNATURE_DESC rsDesc = {};
	rsDesc.pParameters = rootParams; rsDesc.NumParameters = 3;
	rsDesc.pStaticSamplers = &sampler; rsDesc.NumStaticSamplers = 1;
	rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	ComPtr<ID3DBlob> blob, error;
	D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &error);
	device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.pRootSignature = rootSignature_.Get();
	psoDesc.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
	psoDesc.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };

	D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};
	psoDesc.InputLayout = { inputLayout, 1 };

	psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK; // 箱の内側を描く
	psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;

	// 最重要：カリングを NONE に設定
	// これをしないと「箱の内側」にある面がすべて消えてしまいます
	psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;

	// ★スライド15ページの通りに設定：比較するが書き込まない
	psoDesc.DepthStencilState.DepthEnable = true;
	psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

	psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.NumRenderTargets = 1;

	// ★エラー修正：トポロジーの「具体的なルール」ではなく「分類(TYPE)」を指定する
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState_));
}