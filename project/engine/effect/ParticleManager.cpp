#include "effect/ParticleManager.h"
#include "base/DirectXCommon.h"
#include "base/SrvManager.h"
#include "3d/Camera.h"
#include <d3dcompiler.h>
#include <cassert>
#include <random>
#include <cmath>

#pragma comment(lib, "d3dcompiler.lib")

using namespace Microsoft::WRL;

struct VertexData {
    Vector4 position;
    Vector2 texcoord;
    Vector3 normal;
};

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

            float t = p.currentTime / p.lifeTime;
            // 寿命でアルファ値を減衰させる (PS側で色の強さに反映される)
            p.color.w = 1.0f - t;

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
    std::uniform_real_distribution<float> distPos(-0.5f, 0.5f);
    for (uint32_t i = 0; i < count; ++i) {
        Particle& p = AddParticle(name, position);
        p.transform.translate.x += distPos(engine);
        p.transform.translate.y += distPos(engine);
        p.transform.translate.z += distPos(engine);
    }
}

void ParticleManager::Draw() {
    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    commandList->SetPipelineState(pipelineState_.Get());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
    commandList->IASetVertexBuffers(1, 1, &instancingBufferView_);

    Matrix4x4 viewMatrix = camera_->GetViewMatrix();
    Matrix4x4 viewProjectionMatrix = camera_->GetViewProjectionMatrix();

    Matrix4x4 billboardMatrix = MatrixMath::MakeIdentity4x4();
    billboardMatrix.m[0][0] = viewMatrix.m[0][0]; billboardMatrix.m[0][1] = viewMatrix.m[1][0]; billboardMatrix.m[0][2] = viewMatrix.m[2][0];
    billboardMatrix.m[1][0] = viewMatrix.m[0][1]; billboardMatrix.m[1][1] = viewMatrix.m[1][1]; billboardMatrix.m[1][2] = viewMatrix.m[2][1];
    billboardMatrix.m[2][0] = viewMatrix.m[0][2]; billboardMatrix.m[2][1] = viewMatrix.m[1][2]; billboardMatrix.m[2][2] = viewMatrix.m[2][2];

    for (auto& groupPair : particleGroups_) {
        uint32_t instanceIndex = 0;
        for (const auto& p : groupPair.second.particles) {
            if (instanceIndex >= kMaxInstanceCount) break;

            Matrix4x4 scaleMatrix = MatrixMath::MakeScaleMatrix(p.transform.scale);
            Matrix4x4 rotateZMatrix = MatrixMath::MakeRotateZMatrix(p.transform.rotate.z);
            Matrix4x4 translateMatrix = MatrixMath::MakeTranslateMatrix(p.transform.translate);

            Matrix4x4 worldMatrix = MatrixMath::Multiply(scaleMatrix,
                MatrixMath::Multiply(rotateZMatrix,
                    MatrixMath::Multiply(billboardMatrix, translateMatrix)));

            instancingData_[instanceIndex].World = worldMatrix;
            instancingData_[instanceIndex].WVP = MatrixMath::Multiply(worldMatrix, viewProjectionMatrix);
            instancingData_[instanceIndex].color = p.color;
            instanceIndex++;
        }

        if (instanceIndex == 0) continue;
        commandList->SetGraphicsRootDescriptorTable(0, srvManager_->GetGPUDescriptorHandle(groupPair.second.textureHandle));
        commandList->DrawInstanced(6, instanceIndex, 0, 0);
    }
}

void ParticleManager::CreateParticleGroup(const std::string& name, uint32_t textureHandle) {
    ParticleGroup& group = particleGroups_[name];
    group.name = name;
    group.textureHandle = textureHandle;
}

void ParticleManager::CreateRootSignature() {
    D3D12_DESCRIPTOR_RANGE descriptorRange{};
    descriptorRange.BaseShaderRegister = 0;
    descriptorRange.NumDescriptors = 1;
    descriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameters[1] = {};
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[0].DescriptorTable.pDescriptorRanges = &descriptorRange;
    rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;

    D3D12_STATIC_SAMPLER_DESC staticSampler{};
    staticSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    staticSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
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

    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "WVP",      0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "WVP",      1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "WVP",      2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "WVP",      3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "WORLD",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "WORLD",    1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "WORLD",    2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "WORLD",    3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
    };

    // ★真相ポイント1：明示的なブレンド設定の代入
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
    VertexData vertices6[] = {
        {{ -0.5f, -0.5f, 0.0f, 1.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, -1.0f }},
        {{ -0.5f,  0.5f, 0.0f, 1.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f }},
        {{  0.5f, -0.5f, 0.0f, 1.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, -1.0f }},
        {{ -0.5f,  0.5f, 0.0f, 1.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f }},
        {{  0.5f,  0.5f, 0.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f, -1.0f }},
        {{  0.5f, -0.5f, 0.0f, 1.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, -1.0f }},
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