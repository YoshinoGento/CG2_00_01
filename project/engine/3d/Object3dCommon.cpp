#include "Object3dCommon.h"
#include "base/Logger.h"
#include "base/SrvManager.h"
#include "math/Matrix.h"
#include <algorithm>
#include <cassert>
#include <cmath>

void Object3dCommon::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) {
	if (dxCommon == nullptr || srvManager == nullptr) {
		Logger::Log("Object3dCommon::Initialize failed: null dependency.\n");
		assert(false && "Object3dCommon requires DirectXCommon and SrvManager.");
		return;
	}
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
    CreateRootSignature();
    CreateSkinningRootSignature();
    CreateSkinningComputeRootSignature();
	CreateShadowRootSignature();
    CreateGraphicsPipelineStates();
    CreateSkinningComputePipelineState();
	CreateShadowPipelineStates();
	CreateShadowResources();
}

void Object3dCommon::CommonDrawSettings() {
    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void Object3dCommon::UpdateDirectionalShadow(const Vector3& lightDirection, const Vector3& focusPosition) {
	if (!shadowReady_) {
		return;
	}

	Vector3 direction = MatrixMath::Normalize(lightDirection);
	if (MatrixMath::Length(direction) < 0.0001f) {
		direction = MatrixMath::Normalize(Vector3{ 0.0f, -1.0f, 1.0f });
	}

	const float worldUnitsPerTexel = (kShadowHalfExtent * 2.0f) / static_cast<float>(kShadowMapSize);
	Vector3 snappedFocus = focusPosition;
	snappedFocus.x = std::round(snappedFocus.x / worldUnitsPerTexel) * worldUnitsPerTexel;
	snappedFocus.z = std::round(snappedFocus.z / worldUnitsPerTexel) * worldUnitsPerTexel;

	Vector3 up = { 0.0f, 1.0f, 0.0f };
	if (std::abs(MatrixMath::Dot(direction, up)) > 0.98f) {
		up = { 0.0f, 0.0f, 1.0f };
	}
	const Vector3 lightPosition = snappedFocus - direction * kShadowLightDistance;
	const Matrix4x4 lightView = MatrixMath::MakeLookAtMatrix(lightPosition, snappedFocus, up);
	const Matrix4x4 lightProjection = MatrixMath::MakeOrthographicMatrix(
		-kShadowHalfExtent, kShadowHalfExtent, kShadowHalfExtent, -kShadowHalfExtent,
		kShadowNearClip, kShadowFarClip);
	lightViewProjectionMatrix_ = MatrixMath::Multiply(lightView, lightProjection);

	shadowSceneData_->lightViewProjection = lightViewProjectionMatrix_;
	shadowSceneData_->texelSize = {
		1.0f / static_cast<float>(kShadowMapSize),
		1.0f / static_cast<float>(kShadowMapSize)
	};
	shadowSceneData_->depthBias = 0.00035f;
	shadowSceneData_->normalBias = 0.0015f;
	shadowSceneData_->strength = shadowStrength_;
}

bool Object3dCommon::BeginShadowPass() {
	if (!shadowReady_) {
		return false;
	}

	TransitionShadowMap(D3D12_RESOURCE_STATE_DEPTH_WRITE);
	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
	const D3D12_CPU_DESCRIPTOR_HANDLE dsv = shadowDsvHeap_->GetCPUDescriptorHandleForHeapStart();
	commandList->OMSetRenderTargets(0, nullptr, FALSE, &dsv);
	commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	const D3D12_VIEWPORT viewport = {
		0.0f, 0.0f,
		static_cast<float>(kShadowMapSize), static_cast<float>(kShadowMapSize),
		0.0f, 1.0f
	};
	const D3D12_RECT scissor = { 0, 0, static_cast<LONG>(kShadowMapSize), static_cast<LONG>(kShadowMapSize) };
	commandList->RSSetViewports(1, &viewport);
	commandList->RSSetScissorRects(1, &scissor);
	commandList->SetGraphicsRootSignature(shadowRootSignature_.Get());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	return true;
}

void Object3dCommon::EndShadowPass() {
	if (!shadowReady_) {
		return;
	}

	TransitionShadowMap(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	dxCommon_->SetSceneRenderTarget();
	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
	const D3D12_VIEWPORT viewport = {
		0.0f, 0.0f,
		static_cast<float>(WinApp::kClientWidth), static_cast<float>(WinApp::kClientHeight),
		0.0f, 1.0f
	};
	const D3D12_RECT scissor = { 0, 0, WinApp::kClientWidth, WinApp::kClientHeight };
	commandList->RSSetViewports(1, &viewport);
	commandList->RSSetScissorRects(1, &scissor);
}

void Object3dCommon::SetShadowStrength(float strength) {
	shadowStrength_ = std::clamp(strength, 0.0f, 1.0f);
	if (shadowSceneData_) {
		shadowSceneData_->strength = shadowStrength_;
	}
}

D3D12_GPU_VIRTUAL_ADDRESS Object3dCommon::GetShadowSceneBufferAddress() const {
	return shadowSceneResource_ ? shadowSceneResource_->GetGPUVirtualAddress() : 0;
}

D3D12_GPU_DESCRIPTOR_HANDLE Object3dCommon::GetShadowMapSrvHandle() const {
	return shadowReady_ ? srvManager_->GetGPUDescriptorHandle(shadowMapSrvIndex_) : D3D12_GPU_DESCRIPTOR_HANDLE{};
}

void Object3dCommon::CreateRootSignature() {
    ID3D12Device* device = dxCommon_->GetDevice();

    // ★重要：パラメータを 7つ に増やします
    // (0:Material, 1:Transform, 2:DirLight, 3:Camera, 4:SpotLight, 5:Texture, 6:EnvironmentMap)
    D3D12_ROOT_PARAMETER rootParameters[9] = {};

    // 0: Material (Pixel)
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[0].Descriptor.ShaderRegister = 0;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // 1: TransformationMatrix (Vertex)
    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[1].Descriptor.ShaderRegister = 0;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

    // 2: DirectionalLight (Pixel)
    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[2].Descriptor.ShaderRegister = 1;
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // 3: Camera (Pixel)
    rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[3].Descriptor.ShaderRegister = 2;
    rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // 4: SpotLight (Pixel)
    rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[4].Descriptor.ShaderRegister = 3;
    rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // 5: Texture2D (register t0)
    D3D12_DESCRIPTOR_RANGE descriptorRange0[1] = {};
    descriptorRange0[0].BaseShaderRegister = 0; // t0
    descriptorRange0[0].NumDescriptors = 1;
    descriptorRange0[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRange0[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    rootParameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[5].DescriptorTable.pDescriptorRanges = descriptorRange0;
    rootParameters[5].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // 6: TextureCube (register t1) ★環境マップ用に新設
    D3D12_DESCRIPTOR_RANGE descriptorRange1[1] = {};
    descriptorRange1[0].BaseShaderRegister = 1; // t1
    descriptorRange1[0].NumDescriptors = 1;
    descriptorRange1[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRange1[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    rootParameters[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[6].DescriptorTable.pDescriptorRanges = descriptorRange1;
    rootParameters[6].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	rootParameters[7].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[7].Descriptor.ShaderRegister = 4;
	rootParameters[7].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_DESCRIPTOR_RANGE shadowRange[1] = {};
	shadowRange[0].BaseShaderRegister = 3;
	shadowRange[0].NumDescriptors = 1;
	shadowRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	shadowRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	rootParameters[8].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[8].DescriptorTable.pDescriptorRanges = shadowRange;
	rootParameters[8].DescriptorTable.NumDescriptorRanges = 1;
	rootParameters[8].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // --- サンプラーの設定 (s0) ---
    D3D12_STATIC_SAMPLER_DESC staticSamplers[2] = {};
    staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
    staticSamplers[0].ShaderRegister = 0;
    staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	staticSamplers[1].Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
	staticSamplers[1].AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	staticSamplers[1].AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	staticSamplers[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	staticSamplers[1].ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	staticSamplers[1].BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
	staticSamplers[1].MaxLOD = D3D12_FLOAT32_MAX;
	staticSamplers[1].ShaderRegister = 1;
	staticSamplers[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // --- 署名の作成 ---
    D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature = {};
    descriptionRootSignature.pParameters = rootParameters;
    descriptionRootSignature.NumParameters = _countof(rootParameters);
    descriptionRootSignature.pStaticSamplers = staticSamplers;
    descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);
    descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    // シリアライズして作成
    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
    HRESULT hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    if (FAILED(hr)) {
        OutputDebugStringA((char*)errorBlob->GetBufferPointer());
        assert(false);
    }
    hr = device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
    assert(SUCCEEDED(hr));
}

void Object3dCommon::CreateSkinningRootSignature() {
    ID3D12Device* device = dxCommon_->GetDevice();

    D3D12_ROOT_PARAMETER rootParameters[10] = {};

    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[0].Descriptor.ShaderRegister = 0;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[1].Descriptor.ShaderRegister = 0;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[2].Descriptor.ShaderRegister = 1;
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[3].Descriptor.ShaderRegister = 2;
    rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[4].Descriptor.ShaderRegister = 3;
    rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_DESCRIPTOR_RANGE textureRange[1] = {};
    textureRange[0].BaseShaderRegister = 0;
    textureRange[0].NumDescriptors = 1;
    textureRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    textureRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    rootParameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[5].DescriptorTable.pDescriptorRanges = textureRange;
    rootParameters[5].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_DESCRIPTOR_RANGE environmentRange[1] = {};
    environmentRange[0].BaseShaderRegister = 1;
    environmentRange[0].NumDescriptors = 1;
    environmentRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    environmentRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    rootParameters[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[6].DescriptorTable.pDescriptorRanges = environmentRange;
    rootParameters[6].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    rootParameters[7].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[7].Descriptor.ShaderRegister = 4;
    rootParameters[7].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_DESCRIPTOR_RANGE shadowRange[1] = {};
	shadowRange[0].BaseShaderRegister = 3;
	shadowRange[0].NumDescriptors = 1;
	shadowRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	shadowRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	rootParameters[8].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[8].DescriptorTable.pDescriptorRanges = shadowRange;
	rootParameters[8].DescriptorTable.NumDescriptorRanges = 1;
	rootParameters[8].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_DESCRIPTOR_RANGE paletteRange[1] = {};
    paletteRange[0].BaseShaderRegister = 2;
    paletteRange[0].NumDescriptors = 1;
    paletteRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    paletteRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    rootParameters[9].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[9].DescriptorTable.pDescriptorRanges = paletteRange;
    rootParameters[9].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[9].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

    D3D12_STATIC_SAMPLER_DESC staticSamplers[2] = {};
    staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
    staticSamplers[0].ShaderRegister = 0;
    staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	staticSamplers[1].Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
	staticSamplers[1].AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	staticSamplers[1].AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	staticSamplers[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	staticSamplers[1].ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	staticSamplers[1].BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
	staticSamplers[1].MaxLOD = D3D12_FLOAT32_MAX;
	staticSamplers[1].ShaderRegister = 1;
	staticSamplers[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature = {};
    descriptionRootSignature.pParameters = rootParameters;
    descriptionRootSignature.NumParameters = _countof(rootParameters);
    descriptionRootSignature.pStaticSamplers = staticSamplers;
    descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);
    descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
    HRESULT hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    if (FAILED(hr)) {
        OutputDebugStringA((char*)errorBlob->GetBufferPointer());
        assert(false);
    }
    hr = device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&skinningRootSignature_));
    assert(SUCCEEDED(hr));
}

void Object3dCommon::CreateSkinningComputeRootSignature() {
    ID3D12Device* device = dxCommon_->GetDevice();

    D3D12_ROOT_PARAMETER rootParameters[5] = {};

    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[0].Descriptor.ShaderRegister = 0;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_DESCRIPTOR_RANGE inputVertexRange[1] = {};
    inputVertexRange[0].BaseShaderRegister = 0;
    inputVertexRange[0].NumDescriptors = 1;
    inputVertexRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    inputVertexRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[1].DescriptorTable.pDescriptorRanges = inputVertexRange;
    rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_DESCRIPTOR_RANGE influenceRange[1] = {};
    influenceRange[0].BaseShaderRegister = 1;
    influenceRange[0].NumDescriptors = 1;
    influenceRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    influenceRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[2].DescriptorTable.pDescriptorRanges = influenceRange;
    rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_DESCRIPTOR_RANGE paletteRange[1] = {};
    paletteRange[0].BaseShaderRegister = 2;
    paletteRange[0].NumDescriptors = 1;
    paletteRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    paletteRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[3].DescriptorTable.pDescriptorRanges = paletteRange;
    rootParameters[3].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_DESCRIPTOR_RANGE outputVertexRange[1] = {};
    outputVertexRange[0].BaseShaderRegister = 0;
    outputVertexRange[0].NumDescriptors = 1;
    outputVertexRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    outputVertexRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[4].DescriptorTable.pDescriptorRanges = outputVertexRange;
    rootParameters[4].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature = {};
    descriptionRootSignature.pParameters = rootParameters;
    descriptionRootSignature.NumParameters = _countof(rootParameters);
    descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
    HRESULT hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) {
            OutputDebugStringA(static_cast<char*>(errorBlob->GetBufferPointer()));
        }
        assert(false);
    }
    hr = device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&skinningComputeRootSignature_));
    assert(SUCCEEDED(hr));
}

void Object3dCommon::CreateShadowRootSignature() {
	ID3D12Device* device = dxCommon_->GetDevice();

	D3D12_DESCRIPTOR_RANGE paletteRange[1] = {};
	paletteRange[0].BaseShaderRegister = 2;
	paletteRange[0].NumDescriptors = 1;
	paletteRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	paletteRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootParameters[2] = {};
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].Descriptor.ShaderRegister = 0;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[1].DescriptorTable.pDescriptorRanges = paletteRange;
	rootParameters[1].DescriptorTable.NumDescriptorRanges = 1;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

	D3D12_ROOT_SIGNATURE_DESC desc = {};
	desc.pParameters = rootParameters;
	desc.NumParameters = _countof(rootParameters);
	desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
	HRESULT hr = D3D12SerializeRootSignature(
		&desc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr)) {
		if (errorBlob) {
			OutputDebugStringA(static_cast<const char*>(errorBlob->GetBufferPointer()));
		}
		Logger::Log("Object3dCommon::CreateShadowRootSignature failed to serialize.\n");
		assert(false);
		return;
	}

	hr = device->CreateRootSignature(
		0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(),
		IID_PPV_ARGS(&shadowRootSignature_));
	if (FAILED(hr)) {
		Logger::Log("Object3dCommon::CreateShadowRootSignature failed to create root signature.\n");
		assert(false);
	}
}

void Object3dCommon::CreateGraphicsPipelineStates() {
    ID3D12Device* device = dxCommon_->GetDevice(); // ★ここでも device を定義
    auto vs = dxCommon_->CompileShader(L"Resources/shader/Object3D.VS.hlsl", L"vs_6_0");
    auto skinningVs = dxCommon_->CompileShader(L"Resources/shader/Object3D.Skinning.VS.hlsl", L"vs_6_0");
    auto ps = dxCommon_->CompileShader(L"Resources/shader/Object3D.PS.hlsl", L"ps_6_0");
    if (vs == nullptr) {
        Logger::Log("Object3dCommon::CreateGraphicsPipelineStates failed: vertex shader compile failed.\n");
        assert(false);
        return;
    }
    if (skinningVs == nullptr) {
        Logger::Log("Object3dCommon::CreateGraphicsPipelineStates failed: skinning vertex shader compile failed.\n");
        assert(false);
        return;
    }
    if (ps == nullptr) {
        Logger::Log("Object3dCommon::CreateGraphicsPipelineStates failed: pixel shader compile failed.\n");
        assert(false);
        return;
    }

    D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 28, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = rootSignature_.Get();
    psoDesc.InputLayout = { inputLayout, _countof(inputLayout) };
    psoDesc.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
    psoDesc.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };
    psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    psoDesc.BlendState.RenderTarget[1].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    psoDesc.DepthStencilState.DepthEnable = true;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 2;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.RTVFormats[1] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	// Clip geometry outside the camera depth range before depth testing.
	psoDesc.RasterizerState.DepthClipEnable = TRUE;

    D3D12_CULL_MODE cullModes[] = { D3D12_CULL_MODE_NONE, D3D12_CULL_MODE_FRONT, D3D12_CULL_MODE_BACK };
    for (int i = 0; i < 3; ++i) {
        psoDesc.RasterizerState.CullMode = cullModes[i];
        psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        HRESULT hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineStates_[i]));
        assert(SUCCEEDED(hr));
    }

    D3D12_INPUT_ELEMENT_DESC skinningInputLayout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 28, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "WEIGHT",   0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "INDEX",    0, DXGI_FORMAT_R32G32B32A32_SINT,  1, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    psoDesc.pRootSignature = skinningRootSignature_.Get();
    psoDesc.InputLayout = { skinningInputLayout, _countof(skinningInputLayout) };
    psoDesc.VS = { skinningVs->GetBufferPointer(), skinningVs->GetBufferSize() };

    for (int i = 0; i < 3; ++i) {
        psoDesc.RasterizerState.CullMode = cullModes[i];
        psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        HRESULT hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&skinningPipelineStates_[i]));
        assert(SUCCEEDED(hr));
    }
}

void Object3dCommon::CreateSkinningComputePipelineState() {
    ID3D12Device* device = dxCommon_->GetDevice();
    auto cs = dxCommon_->CompileShader(L"Resources/shader/Skinning.CS.hlsl", L"cs_6_0");
    if (cs == nullptr) {
        Logger::Log("Object3dCommon::CreateSkinningComputePipelineState failed: compute shader compile failed.\n");
        assert(false);
        return;
    }

    D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.pRootSignature = skinningComputeRootSignature_.Get();
    psoDesc.CS = { cs->GetBufferPointer(), cs->GetBufferSize() };

    HRESULT hr = device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&skinningComputePipelineState_));
    assert(SUCCEEDED(hr));
}

void Object3dCommon::CreateShadowPipelineStates() {
	ID3D12Device* device = dxCommon_->GetDevice();
	auto vertexShader = dxCommon_->CompileShader(L"Resources/shader/Object3D.VS.hlsl", L"vs_6_0");
	auto skinningVertexShader = dxCommon_->CompileShader(L"Resources/shader/Object3D.Skinning.VS.hlsl", L"vs_6_0");
	if (!vertexShader || !skinningVertexShader || !shadowRootSignature_) {
		Logger::Log("Object3dCommon::CreateShadowPipelineStates failed: shader or root signature is missing.\n");
		assert(false);
		return;
	}

	D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 28, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};
	D3D12_INPUT_ELEMENT_DESC skinningInputLayout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 28, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "WEIGHT",   0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "INDEX",    0, DXGI_FORMAT_R32G32B32A32_SINT,  1, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
	desc.pRootSignature = shadowRootSignature_.Get();
	desc.InputLayout = { inputLayout, _countof(inputLayout) };
	desc.VS = { vertexShader->GetBufferPointer(), vertexShader->GetBufferSize() };
	desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	desc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	desc.RasterizerState.DepthBias = 1200;
	desc.RasterizerState.DepthBiasClamp = 0.0f;
	desc.RasterizerState.SlopeScaledDepthBias = 1.5f;
	desc.RasterizerState.DepthClipEnable = TRUE;
	desc.DepthStencilState.DepthEnable = TRUE;
	desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	desc.DepthStencilState.StencilEnable = FALSE;
	desc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	desc.NumRenderTargets = 0;
	desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	desc.SampleDesc.Count = 1;

	HRESULT hr = device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&shadowPipelineState_));
	if (FAILED(hr)) {
		Logger::Log("Object3dCommon::CreateShadowPipelineStates failed for static geometry.\n");
		assert(false);
		return;
	}

	desc.InputLayout = { skinningInputLayout, _countof(skinningInputLayout) };
	desc.VS = { skinningVertexShader->GetBufferPointer(), skinningVertexShader->GetBufferSize() };
	hr = device->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&shadowSkinningPipelineState_));
	if (FAILED(hr)) {
		Logger::Log("Object3dCommon::CreateShadowPipelineStates failed for skinned geometry.\n");
		assert(false);
	}
}

void Object3dCommon::CreateShadowResources() {
	ID3D12Device* device = dxCommon_->GetDevice();

	D3D12_HEAP_PROPERTIES heapProperties = {};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
	heapProperties.CreationNodeMask = 1;
	heapProperties.VisibleNodeMask = 1;

	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDesc.Width = kShadowMapSize;
	resourceDesc.Height = kShadowMapSize;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = DXGI_FORMAT_R32_TYPELESS;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE clearValue = {};
	clearValue.Format = DXGI_FORMAT_D32_FLOAT;
	clearValue.DepthStencil.Depth = 1.0f;
	clearValue.DepthStencil.Stencil = 0;

	HRESULT hr = device->CreateCommittedResource(
		&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &clearValue,
		IID_PPV_ARGS(&shadowMapResource_));
	if (FAILED(hr)) {
		Logger::Log("Object3dCommon::CreateShadowResources failed to create shadow texture.\n");
		assert(false);
		return;
	}

	shadowDsvHeap_ = dxCommon_->CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);
	if (!shadowDsvHeap_) {
		Logger::Log("Object3dCommon::CreateShadowResources failed to create DSV heap.\n");
		assert(false);
		return;
	}
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	device->CreateDepthStencilView(
		shadowMapResource_.Get(), &dsvDesc,
		shadowDsvHeap_->GetCPUDescriptorHandleForHeapStart());

	shadowMapSrvIndex_ = srvManager_->Allocate();
	srvManager_->CreateSRVforTexture2D(
		shadowMapSrvIndex_, shadowMapResource_.Get(), DXGI_FORMAT_R32_FLOAT, 1);

	shadowSceneResource_ = dxCommon_->CreateBufferResource(sizeof(ShadowSceneData));
	if (!shadowSceneResource_) {
		Logger::Log("Object3dCommon::CreateShadowResources failed to create scene constant buffer.\n");
		assert(false);
		return;
	}
	hr = shadowSceneResource_->Map(0, nullptr, reinterpret_cast<void**>(&shadowSceneData_));
	if (FAILED(hr) || shadowSceneData_ == nullptr) {
		Logger::Log("Object3dCommon::CreateShadowResources failed to map scene constant buffer.\n");
		assert(false);
		return;
	}

	lightViewProjectionMatrix_ = MatrixMath::MakeIdentity4x4();
	shadowSceneData_->lightViewProjection = lightViewProjectionMatrix_;
	shadowSceneData_->texelSize = {
		1.0f / static_cast<float>(kShadowMapSize),
		1.0f / static_cast<float>(kShadowMapSize)
	};
	shadowSceneData_->depthBias = 0.00035f;
	shadowSceneData_->normalBias = 0.0015f;
	shadowSceneData_->strength = shadowStrength_;
	shadowSceneData_->padding[0] = 0.0f;
	shadowSceneData_->padding[1] = 0.0f;
	shadowSceneData_->padding[2] = 0.0f;
	shadowMapState_ = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	shadowReady_ = true;
}

void Object3dCommon::TransitionShadowMap(D3D12_RESOURCE_STATES stateAfter) {
	if (!shadowMapResource_ || shadowMapState_ == stateAfter) {
		return;
	}

	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = shadowMapResource_.Get();
	barrier.Transition.StateBefore = shadowMapState_;
	barrier.Transition.StateAfter = stateAfter;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	dxCommon_->GetCommandList()->ResourceBarrier(1, &barrier);
	shadowMapState_ = stateAfter;
}
