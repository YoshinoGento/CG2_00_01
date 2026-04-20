#include "ParticleManager.h"
#include "DirectXCommon.h"
#include "SrvManager.h"
#include "Camera.h"
#include <d3dcompiler.h>
#include <cassert>
#include <random>
#include <cmath>

#pragma comment(lib, "d3dcompiler.lib")

using namespace Microsoft::WRL;

// ★修正：エラー C2065 の解消。頂点データの構造体をここで定義します
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

            // 速度と加速度の計算
            p.velocity.x += p.acceleration.x;
            p.velocity.y += p.acceleration.y;
            p.velocity.z += p.acceleration.z;
            p.transform.translate.x += p.velocity.x;
            p.transform.translate.y += p.velocity.y;
            p.transform.translate.z += p.velocity.z;

            // Emit 用のサイズ補間（AddParticleで直接scaleをいじらない場合のみ影響）
            float t = p.currentTime / p.lifeTime;
            if (p.startSize != p.endSize) {
                float s = p.startSize + (p.endSize - p.startSize) * t;
                p.transform.scale = { s, s, s };
            }

            // アルファフェードアウト
            p.color.w = 1.0f - t;

            ++it;
        }
    }
}

/**
 * 新しい「AddParticle」
 */
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
    p.startSize = 1.0f; // 同値にしておけばUpdateの補間を無視できる
    p.endSize = 1.0f;

    return p;
}

/**
 * 従来の「Emit」
 */
void ParticleManager::Emit(const std::string& name, const Vector3& position, uint32_t count) {
    std::random_device seed_gen;
    std::mt19937 engine(seed_gen());
    std::uniform_real_distribution<float> distPos(-0.5f, 0.5f);
    std::uniform_real_distribution<float> distVel(-0.1f, 0.1f);

    for (uint32_t i = 0; i < count; ++i) {
        Particle& p = AddParticle(name, position);
        p.transform.translate.x += distPos(engine);
        p.transform.translate.y += distPos(engine);
        p.transform.translate.z += distPos(engine);
        p.velocity = { distVel(engine), distVel(engine), distVel(engine) };
        p.lifeTime = 1.0f;
        p.startSize = 1.0f;
        p.endSize = 0.0f; // 消えていく設定
    }
}

void ParticleManager::Draw() {
    // ... 中略 ... 
    // ※行列計算部分で、以下の順番で計算するのがコツです
    // Matrix4x4 worldMatrix = Scale * Billboard * RotateZ * Translate;
    // これにより、カメラを向きつつ画面上で回転させることができます（火花に必須）

    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    commandList->SetPipelineState(pipelineState_.Get());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
    commandList->IASetVertexBuffers(1, 1, &instancingBufferView_);

    Matrix4x4 viewMatrix = camera_->GetViewMatrix();
    Matrix4x4 viewProjectionMatrix = camera_->GetViewProjectionMatrix();

    // ビルボード行列の抽出
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

            // 合成：Scale * Billboard * RotateZ * Translate
            Matrix4x4 worldMatrix = MatrixMath::Multiply(scaleMatrix,
                MatrixMath::Multiply(billboardMatrix,
                    MatrixMath::Multiply(rotateZMatrix, translateMatrix)));

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
    if (particleGroups_.contains(name)) return;
    ParticleGroup group;
    group.name = name;
    group.textureHandle = textureHandle;
    particleGroups_[name] = group;
}

void ParticleManager::CreateRootSignature() {
    HRESULT hr;

    // ディスクリプタレンジ
    D3D12_DESCRIPTOR_RANGE descriptorRange{};
    descriptorRange.BaseShaderRegister = 0; // t0
    descriptorRange.NumDescriptors = 1;
    descriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    // ルートパラメータ
    D3D12_ROOT_PARAMETER rootParameters[1] = {};

    // 0: テクスチャ (DescriptorTable)
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[0].DescriptorTable.pDescriptorRanges = &descriptorRange;
    rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;

    // サンプラー
    D3D12_STATIC_SAMPLER_DESC staticSampler{};
    staticSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    staticSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSampler.ShaderRegister = 0; // s0
    staticSampler.MaxLOD = D3D12_FLOAT32_MAX;
    staticSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // ルートシグネチャ記述子
    D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
    descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    descriptionRootSignature.pParameters = rootParameters;
    descriptionRootSignature.NumParameters = _countof(rootParameters);
    descriptionRootSignature.pStaticSamplers = &staticSampler;
    descriptionRootSignature.NumStaticSamplers = 1;

    // シリアライズ
    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
    hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    if (FAILED(hr)) {
        assert(false);
    }

    // 生成
    hr = dxCommon_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
    assert(SUCCEEDED(hr));
}

void ParticleManager::CreateGraphicsPipelineState() {
    HRESULT hr;

    // ★修正: ファイルパスを "Resources/shader/" に変更
    // 必ずファイルを Resources/shader/ フォルダに移動してから実行してください
    Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = dxCommon_->CompileShader(L"Resources/shader/Particle.VS.hlsl", L"vs_6_0");
    assert(vertexShaderBlob != nullptr);

    Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = dxCommon_->CompileShader(L"Resources/shader/Particle.PS.hlsl", L"ps_6_0");
    assert(pixelShaderBlob != nullptr);

    // インプットレイアウト
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
        // 頂点データ (Slot 0)
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },

        // インスタンシングデータ (Slot 1)
        // WVP行列 (4x4)
        { "WVP",      0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "WVP",      1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "WVP",      2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "WVP",      3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        // World行列 (4x4)
        { "WORLD",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "WORLD",    1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "WORLD",    2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "WORLD",    3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        // 色
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
    };

    // ブレンドステート (加算合成)
    D3D12_BLEND_DESC blendDesc{};
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE; // 加算
    blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    // ラスタライザステート
    D3D12_RASTERIZER_DESC rasterizerDesc{};
    rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE; // 両面描画
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

    // デプスステンシルステート
    D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
    depthStencilDesc.DepthEnable = TRUE;
    depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; // 深度書き込みしない
    depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

    // PSO生成
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.pRootSignature = rootSignature_.Get();
    psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
    psoDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
    psoDesc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };
    psoDesc.BlendState = blendDesc;
    psoDesc.RasterizerState = rasterizerDesc;
    psoDesc.DepthStencilState = depthStencilDesc;
    psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

    hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState_));
    assert(SUCCEEDED(hr));
}

void ParticleManager::CreateModel() {
    // 矩形の頂点データ（フルクアッド、UV を矩形に修正）
    VertexData vertices6[] = {
        // 三角形1
        {{ -0.5f, -0.5f, 0.0f, 1.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, -1.0f }}, // 左下 (0,1)
        {{ -0.5f,  0.5f, 0.0f, 1.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f }}, // 左上 (0,0)
        {{  0.5f, -0.5f, 0.0f, 1.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, -1.0f }}, // 右下 (1,1)
        // 三角形2
        {{ -0.5f,  0.5f, 0.0f, 1.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f }}, // 左上 (0,0)
        {{  0.5f,  0.5f, 0.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f, -1.0f }}, // 右上 (1,0)
        {{  0.5f, -0.5f, 0.0f, 1.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, -1.0f }}, // 右下 (1,1)
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