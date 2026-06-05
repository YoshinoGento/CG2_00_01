#include "effect/ParticleManager.h"
#include "base/DirectXCommon.h"
#include "base/SrvManager.h"
#include "3d/Camera.h"
#include "3d/Model.h"
#include <d3dcompiler.h>
#include <cassert>
#include <cstring>
#include <random>
#include <cmath>

#pragma comment(lib, "d3dcompiler.lib")

using namespace Microsoft::WRL;

// Model::VertexData と同じフィールド順にする（position → normal → texcoord）
// ※ Input Layout のメモリ配置と一致させる必要がある
struct VertexData {
    Vector4 position;
    Vector3 normal;
    Vector2 texcoord;
};

static void TransitionResource(
    ID3D12GraphicsCommandList* commandList,
    ID3D12Resource* resource,
    D3D12_RESOURCE_STATES before,
    D3D12_RESOURCE_STATES after) {
    assert(commandList);
    assert(resource);
    if (before == after) {
        return;
    }

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = resource;
    barrier.Transition.StateBefore = before;
    barrier.Transition.StateAfter = after;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->ResourceBarrier(1, &barrier);
}

void ParticleManager::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) {
    assert(dxCommon);
    assert(srvManager);
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;

    CreateRootSignature();
    CreateGraphicsPipelineState();
    CreateModel();

    instancingResource_ = dxCommon_->CreateBufferResource(sizeof(InstancingData) * kMaxInstanceCount);
    instancingResource_->Map(0, nullptr, (void**)&instancingData_);

    instancingBufferView_.BufferLocation = instancingResource_->GetGPUVirtualAddress();
    instancingBufferView_.SizeInBytes = sizeof(InstancingData) * kMaxInstanceCount;
    instancingBufferView_.StrideInBytes = sizeof(InstancingData);

    // === discardしきい値用の定数バッファ（CBuffer）を作成 ===
    // 資料スライド10: alphaReference をシェーダーに渡すためのバッファ
    // GPU側の register(b0) にバインドされる
    materialResource_ = dxCommon_->CreateBufferResource(sizeof(MaterialData));
    materialResource_->Map(0, nullptr, (void**)&materialData_);
    CreateGPUParticleResources();
    CreateGPUParticleComputeRootSignature();
    CreateGPUParticleComputePipelineState();
    CreateGPUParticleGraphicsRootSignature();
    CreateGPUParticleGraphicsPipelineState();
    materialData_->alphaReference = 0.0f; // 初期値: 0（従来と同じ動作）
}

void ParticleManager::Update(Camera* camera) {
    camera_ = camera;
    float deltaTime = 1.0f / 60.0f;

    for (auto& groupPair : particleGroups_) {
        for (auto it = groupPair.second.particles.begin(); it != groupPair.second.particles.end();) {
            Particle& p = *it;
            p.currentTime += deltaTime;

            if (p.currentTime >= p.lifeTime) {
                it = groupPair.second.particles.erase(it);
                continue;
            }

            p.velocity.x += p.acceleration.x;
            p.velocity.y += p.acceleration.y;
            p.velocity.z += p.acceleration.z;
            p.transform.translate.x += p.velocity.x;
            p.transform.translate.y += p.velocity.y;
            p.transform.translate.z += p.velocity.z;

			// UVスクロールのアニメーション
			p.uvOffset.x += p.uvVelocity.x * deltaTime;
			p.uvOffset.y += p.uvVelocity.y * deltaTime;

            float t = p.currentTime / p.lifeTime;
            // 寿命でアルファ値を減衰させる (PS側で色の強さに反映される)
            p.color.w = 1.0f - t;

            // 寿命に応じてスケールを補間する（Ringの拡張アニメーションに必須）
            float currentSize = p.startSize + (p.endSize - p.startSize) * t;
			p.transform.scale = { currentSize,currentSize,currentSize };


            ++it;
        }
    }
}

Particle& ParticleManager::AddParticle(const std::string& name, const Vector3& position) {
    auto it = particleGroups_.find(name);
    assert(it != particleGroups_.end());
    it->second.particles.emplace_back();
    Particle& p = it->second.particles.back();
    p.transform.translate = position;
    p.transform.rotate = { 0.0f, 0.0f, 0.0f };
    p.transform.scale = { 1.0f, 1.0f, 1.0f };
    p.velocity = { 0.0f, 0.0f, 0.0f };
    p.acceleration = { 0.0f, 0.0f, 0.0f };
    p.color = { 1.0f, 1.0f, 1.0f, 1.0f };
    p.lifeTime = 1.0f;
    p.currentTime = 0.0f;
    return p;
}

void ParticleManager::Emit(const std::string& name, const Vector3& position, uint32_t count) {
    std::random_device seed_gen;
    std::mt19937 engine(seed_gen());
    std::uniform_real_distribution<float> distPos(-0.3f, 0.3f);
    std::uniform_real_distribution<float> distVel(-0.1f, 0.1f);
    std::uniform_real_distribution<float> distScale(0.1f, 0.4f);

    for (uint32_t i = 0; i < count; ++i) {
        Particle& p = AddParticle(name, position);
        // ランダムなオフセットと速度を付与
        p.transform.translate.x += distPos(engine);
        p.transform.translate.y += distPos(engine);
        p.transform.translate.z += distPos(engine);

        p.velocity = { distVel(engine), distVel(engine) + 0.1f, distVel(engine) };
        p.transform.scale = { distScale(engine), distScale(engine), 1.0f };
        p.lifeTime = 0.5f;
        p.color = { 1.0f, 0.8f, 0.2f, 1.0f }; // 火の粉のような色
    }
}
void ParticleManager::ClearAll() {
    for (auto& pair : particleGroups_) {
        pair.second.particles.clear();
    }
}

void ParticleManager::Draw() {
    // CPU側の設定値をGPUバッファに反映
    materialData_->alphaReference = alphaReference_;

    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    commandList->SetPipelineState(pipelineState_.Get());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
    commandList->IASetVertexBuffers(1, 1, &instancingBufferView_);

    // CBufferをシェーダーのregister(b0)にバインド
    // Root Parameter Index 1 に対応（0番はテクスチャSRV）
    commandList->SetGraphicsRootConstantBufferView(1, materialResource_->GetGPUVirtualAddress());

    Matrix4x4 viewMatrix = camera_->GetViewMatrix();
    Matrix4x4 viewProjectionMatrix = camera_->GetViewProjectionMatrix();

    Matrix4x4 billboardMatrix = MatrixMath::MakeIdentity4x4();
    billboardMatrix.m[0][0] = viewMatrix.m[0][0]; billboardMatrix.m[0][1] = viewMatrix.m[1][0]; billboardMatrix.m[0][2] = viewMatrix.m[2][0];
    billboardMatrix.m[1][0] = viewMatrix.m[0][1]; billboardMatrix.m[1][1] = viewMatrix.m[1][1]; billboardMatrix.m[1][2] = viewMatrix.m[2][1];
    billboardMatrix.m[2][0] = viewMatrix.m[0][2]; billboardMatrix.m[2][1] = viewMatrix.m[1][2]; billboardMatrix.m[2][2] = viewMatrix.m[2][2];

    for (auto& groupPair : particleGroups_) {
        ParticleGroup& group = groupPair.second;
        uint32_t instanceIndex = 0;
        for (const auto& p : group.particles) {
            if (instanceIndex >= kMaxInstanceCount) break;

            // === ワールド行列の構築 ===
            // MakeAffineMatrix は Scale * RotateXYZ * Translate を一括計算する
            // これにより X/Y/Z 全軸の回転が正しく反映される
            // （旧コードは rotateZ しか使っておらず、X/Y軸回転が無視されていた）
            Matrix4x4 worldMatrix;
            if (group.model) {
                // モデル指定時（Ring, Cylinder等）: XYZ回転をそのまま使う
                // ビルボードは不要（3Dモデルは常にワールド空間で回転させる）
                worldMatrix = MatrixMath::MakeAffineMatrix(
                    p.transform.scale, p.transform.rotate, p.transform.translate);
            }
            else {
                // 四角形スプライト時: ビルボード + Z軸回転
                // カメラの方を向く行列を使って常にカメラ正面に表示する
                Matrix4x4 scaleMatrix = MatrixMath::MakeScaleMatrix(p.transform.scale);
                Matrix4x4 rotateZMatrix = MatrixMath::MakeRotateZMatrix(p.transform.rotate.z);
                Matrix4x4 translateMatrix = MatrixMath::MakeTranslateMatrix(p.transform.translate);
                worldMatrix = MatrixMath::Multiply(scaleMatrix,
                    MatrixMath::Multiply(rotateZMatrix,
                        MatrixMath::Multiply(billboardMatrix, translateMatrix)));
            }

            instancingData_[instanceIndex].World = worldMatrix;
            instancingData_[instanceIndex].WVP = MatrixMath::Multiply(worldMatrix, viewProjectionMatrix);
            instancingData_[instanceIndex].color = p.color;
			//UVスケールとオフセットをInstanceingDataに設定（xy: Scale、zw:Offset）
			instancingData_[instanceIndex].uvTransform = { p.uvScale.x, p.uvScale.y, p.uvOffset.x, p.uvOffset.y };
            instanceIndex++;
        }
        if (instanceIndex == 0) continue;
        commandList->SetGraphicsRootDescriptorTable(0, srvManager_->GetGPUDescriptorHandle(group.textureHandle));

        if (group.model) {
            group.model->Draw(commandList, instanceIndex);
        }
        else {
            commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
            commandList->DrawInstanced(6, instanceIndex, 0, 0);
        }
    }

    DrawGPUParticles();
}

void ParticleManager::CreateGPUParticleResources() {
    gpuParticleResource_ = dxCommon_->CreateUAVBufferResource(
        sizeof(ParticleCS) * kMaxGPUParticleCount,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    gpuParticleResourceState_ = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

    gpuParticleSrvHandle_ = srvManager_->Allocate();
    srvManager_->CreateSRVforStructuredBuffer(
        gpuParticleSrvHandle_,
        gpuParticleResource_.Get(),
        kMaxGPUParticleCount,
        sizeof(ParticleCS));

    gpuParticleUavHandle_ = srvManager_->Allocate();
    srvManager_->CreateUAVforStructuredBuffer(
        gpuParticleUavHandle_,
        gpuParticleResource_.Get(),
        kMaxGPUParticleCount,
        sizeof(ParticleCS));

    gpuParticleViewResource_ = dxCommon_->CreateBufferResource(sizeof(GPUParticleViewData));
    HRESULT hr = gpuParticleViewResource_->Map(0, nullptr, reinterpret_cast<void**>(&gpuParticleViewData_));
    assert(SUCCEEDED(hr));
    gpuParticleViewData_->viewProjection = MatrixMath::MakeIdentity4x4();
    gpuParticleViewData_->billboardMatrix = MatrixMath::MakeIdentity4x4();
}

void ParticleManager::CreateGPUParticleComputeRootSignature() {
    D3D12_DESCRIPTOR_RANGE particleUavRange{};
    particleUavRange.BaseShaderRegister = 0;
    particleUavRange.NumDescriptors = 1;
    particleUavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    particleUavRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameter{};
    rootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    rootParameter.DescriptorTable.pDescriptorRanges = &particleUavRange;
    rootParameter.DescriptorTable.NumDescriptorRanges = 1;

    D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
    descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
    descriptionRootSignature.pParameters = &rootParameter;
    descriptionRootSignature.NumParameters = 1;

    ComPtr<ID3DBlob> signatureBlob;
    ComPtr<ID3DBlob> errorBlob;
    HRESULT hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) {
            OutputDebugStringA(static_cast<char*>(errorBlob->GetBufferPointer()));
        }
        assert(false);
    }
    hr = dxCommon_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&gpuParticleComputeRootSignature_));
    assert(SUCCEEDED(hr));
}

void ParticleManager::CreateGPUParticleComputePipelineState() {
    ComPtr<IDxcBlob> csBlob = dxCommon_->CompileShader(L"Resources/shader/InitializeParticle.CS.hlsl", L"cs_6_0");

    D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.pRootSignature = gpuParticleComputeRootSignature_.Get();
    psoDesc.CS = { csBlob->GetBufferPointer(), csBlob->GetBufferSize() };

    HRESULT hr = dxCommon_->GetDevice()->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&gpuParticleComputePipelineState_));
    assert(SUCCEEDED(hr));
}

void ParticleManager::CreateGPUParticleGraphicsRootSignature() {
    D3D12_DESCRIPTOR_RANGE textureRange{};
    textureRange.BaseShaderRegister = 0;
    textureRange.NumDescriptors = 1;
    textureRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    textureRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_DESCRIPTOR_RANGE particleSrvRange{};
    particleSrvRange.BaseShaderRegister = 0;
    particleSrvRange.NumDescriptors = 1;
    particleSrvRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    particleSrvRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameters[4] = {};
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[0].DescriptorTable.pDescriptorRanges = &textureRange;
    rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;

    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[1].Descriptor.ShaderRegister = 0;

    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParameters[2].DescriptorTable.pDescriptorRanges = &particleSrvRange;
    rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;

    rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParameters[3].Descriptor.ShaderRegister = 0;

    D3D12_STATIC_SAMPLER_DESC staticSampler{};
    staticSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    staticSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSampler.ShaderRegister = 0;
    staticSampler.MaxLOD = D3D12_FLOAT32_MAX;
    staticSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
    descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    descriptionRootSignature.pParameters = rootParameters;
    descriptionRootSignature.NumParameters = _countof(rootParameters);
    descriptionRootSignature.pStaticSamplers = &staticSampler;
    descriptionRootSignature.NumStaticSamplers = 1;

    ComPtr<ID3DBlob> signatureBlob;
    ComPtr<ID3DBlob> errorBlob;
    HRESULT hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) {
            OutputDebugStringA(static_cast<char*>(errorBlob->GetBufferPointer()));
        }
        assert(false);
    }
    hr = dxCommon_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&gpuParticleGraphicsRootSignature_));
    assert(SUCCEEDED(hr));
}

void ParticleManager::CreateGPUParticleGraphicsPipelineState() {
    ComPtr<IDxcBlob> vsBlob = dxCommon_->CompileShader(L"Resources/shader/GPUParticle.VS.hlsl", L"vs_6_0");
    ComPtr<IDxcBlob> psBlob = dxCommon_->CompileShader(L"Resources/shader/Particle.PS.hlsl", L"ps_6_0");

    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    D3D12_BLEND_DESC blendDesc{};
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
    blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ZERO;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    D3D12_RASTERIZER_DESC rasterizerDesc{};
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
    rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
    rasterizerDesc.DepthClipEnable = TRUE;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.pRootSignature = gpuParticleGraphicsRootSignature_.Get();
    psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
    psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
    psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };
    psoDesc.BlendState = blendDesc;
    psoDesc.RasterizerState = rasterizerDesc;
    psoDesc.DepthStencilState = { TRUE, D3D12_DEPTH_WRITE_MASK_ZERO, D3D12_COMPARISON_FUNC_LESS_EQUAL, FALSE };
    psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.NumRenderTargets = 1;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

    HRESULT hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&gpuParticleGraphicsPipelineState_));
    assert(SUCCEEDED(hr));
}

void ParticleManager::InitializeGPUParticles() {
    if (gpuParticlesInitialized_) {
        return;
    }

    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
    TransitionResource(
        commandList,
        gpuParticleResource_.Get(),
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    gpuParticleResourceState_ = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

    commandList->SetComputeRootSignature(gpuParticleComputeRootSignature_.Get());
    commandList->SetPipelineState(gpuParticleComputePipelineState_.Get());
    commandList->SetComputeRootDescriptorTable(0, srvManager_->GetGPUDescriptorHandle(gpuParticleUavHandle_));
    commandList->Dispatch(1, 1, 1);

    TransitionResource(
        commandList,
        gpuParticleResource_.Get(),
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    gpuParticleResourceState_ = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    gpuParticlesInitialized_ = true;
}

void ParticleManager::DrawGPUParticles() {
    if (!camera_) {
        return;
    }

    uint32_t textureHandle = 0;
    if (!TryGetGPUParticleTextureHandle(textureHandle)) {
        return;
    }

    InitializeGPUParticles();

    Matrix4x4 viewMatrix = camera_->GetViewMatrix();
    Matrix4x4 billboardMatrix = MatrixMath::MakeIdentity4x4();
    billboardMatrix.m[0][0] = viewMatrix.m[0][0]; billboardMatrix.m[0][1] = viewMatrix.m[1][0]; billboardMatrix.m[0][2] = viewMatrix.m[2][0];
    billboardMatrix.m[1][0] = viewMatrix.m[0][1]; billboardMatrix.m[1][1] = viewMatrix.m[1][1]; billboardMatrix.m[1][2] = viewMatrix.m[2][1];
    billboardMatrix.m[2][0] = viewMatrix.m[0][2]; billboardMatrix.m[2][1] = viewMatrix.m[1][2]; billboardMatrix.m[2][2] = viewMatrix.m[2][2];

    gpuParticleViewData_->viewProjection = camera_->GetViewProjectionMatrix();
    gpuParticleViewData_->billboardMatrix = billboardMatrix;

    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
    commandList->SetGraphicsRootSignature(gpuParticleGraphicsRootSignature_.Get());
    commandList->SetPipelineState(gpuParticleGraphicsPipelineState_.Get());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
    commandList->SetGraphicsRootDescriptorTable(0, srvManager_->GetGPUDescriptorHandle(textureHandle));
    commandList->SetGraphicsRootConstantBufferView(1, materialResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootDescriptorTable(2, srvManager_->GetGPUDescriptorHandle(gpuParticleSrvHandle_));
    commandList->SetGraphicsRootConstantBufferView(3, gpuParticleViewResource_->GetGPUVirtualAddress());
    commandList->DrawInstanced(6, kMaxGPUParticleCount, 0, 0);
}

bool ParticleManager::TryGetGPUParticleTextureHandle(uint32_t& textureHandle) const {
    for (const auto& groupPair : particleGroups_) {
        if (!groupPair.second.model) {
            textureHandle = groupPair.second.textureHandle;
            return true;
        }
    }

    if (!particleGroups_.empty()) {
        textureHandle = particleGroups_.begin()->second.textureHandle;
        return true;
    }

    return false;
}

void ParticleManager::CreateParticleGroup(const std::string& name, uint32_t textureHandle, Model* model) {
    ParticleGroup& group = particleGroups_[name];
    group.name = name;
    group.textureHandle = textureHandle;
	group.model = model;
}

void ParticleManager::CreateRootSignature() {
    D3D12_DESCRIPTOR_RANGE descriptorRange{};
    descriptorRange.BaseShaderRegister = 0;
    descriptorRange.NumDescriptors = 1;
    descriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    // === Root Parameter の定義 ===
    // [0] テクスチャ用のSRV（既存）
    // [1] マテリアル定数バッファ用のCBV（新規追加 — alphaReferenceを渡す）
    D3D12_ROOT_PARAMETER rootParameters[2] = {};

    // Root Parameter 0: テクスチャ（ピクセルシェーダーの register(t0)）
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[0].DescriptorTable.pDescriptorRanges = &descriptorRange;
    rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;

    // Root Parameter 1: マテリアルCBuffer（ピクセルシェーダーの register(b0)）
    // alphaReference をシェーダーに渡すために追加
    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[1].Descriptor.ShaderRegister = 0; // register(b0)

    D3D12_STATIC_SAMPLER_DESC staticSampler{};
    staticSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    staticSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;   // 横方向はループさせる
    staticSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;  // 縦方向（内側・外側）は端で止める
    staticSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSampler.ShaderRegister = 0;
    staticSampler.MaxLOD = D3D12_FLOAT32_MAX;
    staticSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
    descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    descriptionRootSignature.pParameters = rootParameters;
    descriptionRootSignature.NumParameters = _countof(rootParameters);
    descriptionRootSignature.pStaticSamplers = &staticSampler;
    descriptionRootSignature.NumStaticSamplers = 1;

    ComPtr<ID3DBlob> signatureBlob, errorBlob;
    D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    dxCommon_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
}

void ParticleManager::CreateGraphicsPipelineState() {
    ComPtr<IDxcBlob> vsBlob = dxCommon_->CompileShader(L"Resources/shader/Particle.VS.hlsl", L"vs_6_0");
    ComPtr<IDxcBlob> psBlob = dxCommon_->CompileShader(L"Resources/shader/Particle.PS.hlsl", L"ps_6_0");

    // Input Layout のメモリ配置は Model::VertexData と同じ順序にする
    // position(float4) → normal(float3) → texcoord(float2)
    // ※ 以前は TEXCOORD と NORMAL が逆順で、3Dモデル(Cylinder等)のUVが壊れていた
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "WVP",      0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "WVP",      1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "WVP",      2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "WVP",      3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "WORLD",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "WORLD",    1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "WORLD",    2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "WORLD",    3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        // UVスクロール・スケール用のパラメータ (TEXCOORDの1番として送る)
        { "TEXCOORD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
    };

    // 明示的なブレンド設定の代入
    D3D12_BLEND_DESC blendDesc{};
    blendDesc.AlphaToCoverageEnable = FALSE;
    blendDesc.IndependentBlendEnable = FALSE;
    blendDesc.RenderTarget[0].BlendEnable = TRUE;

    // カラーの加算設定 (Src * SrcAlpha + Dest * 1)
    blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
    blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;

    // 【修正箇所】アルファは描画先の値を維持させる (Src * 0 + Dest * 1)
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ZERO;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;

    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    // ★真相ポイント2：ラスタライザ設定を完全に固定
    D3D12_RASTERIZER_DESC rasterizerDesc{};
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
    rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE; // カリングなし
    rasterizerDesc.FrontCounterClockwise = FALSE;
    rasterizerDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
    rasterizerDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
    rasterizerDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
    rasterizerDesc.DepthClipEnable = TRUE;
    rasterizerDesc.MultisampleEnable = FALSE;
    rasterizerDesc.AntialiasedLineEnable = FALSE;
    rasterizerDesc.ForcedSampleCount = 0;
    rasterizerDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.pRootSignature = rootSignature_.Get();
    psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
    psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
    psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };
    psoDesc.BlendState = blendDesc;
    psoDesc.RasterizerState = rasterizerDesc;
    psoDesc.DepthStencilState = { TRUE, D3D12_DEPTH_WRITE_MASK_ZERO, D3D12_COMPARISON_FUNC_LESS_EQUAL, FALSE };
    psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    // RTV形式は Game.cpp の srvManager_->CreateSRVforTexture2D に渡している形式と一致させる必要があります
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.NumRenderTargets = 1;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

    HRESULT hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState_));
    assert(SUCCEEDED(hr));
}

void ParticleManager::CreateModel() {
    // 頂点データも VertexData の新しいフィールド順（position, normal, texcoord）に合わせる
    VertexData vertices6[] = {
        {{ -0.5f, -0.5f, 0.0f, 1.0f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 1.0f }},
        {{ -0.5f,  0.5f, 0.0f, 1.0f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f }},
        {{  0.5f, -0.5f, 0.0f, 1.0f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 1.0f }},
        {{ -0.5f,  0.5f, 0.0f, 1.0f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f }},
        {{  0.5f,  0.5f, 0.0f, 1.0f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 0.0f }},
        {{  0.5f, -0.5f, 0.0f, 1.0f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 1.0f }},
    };
    vertexResource_ = dxCommon_->CreateBufferResource(sizeof(VertexData) * 6);
    vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = sizeof(VertexData) * 6;
    vertexBufferView_.StrideInBytes = sizeof(VertexData);
    VertexData* data = nullptr;
    vertexResource_->Map(0, nullptr, (void**)&data);
    std::memcpy(data, vertices6, sizeof(VertexData) * 6);
    vertexResource_->Unmap(0, nullptr);
}
