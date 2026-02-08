#include "ParticleManager.h"
#include "DirectXCommon.h"
#include "SrvManager.h"
#include "Camera.h"
#include <d3dcompiler.h>
#include <cassert>
#include <random>

#pragma comment(lib, "d3dcompiler.lib")

using namespace Microsoft::WRL;

// 頂点データの構造体
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

    // 1. パイプラインステートの生成
    CreateRootSignature();
    CreateGraphicsPipelineState();

    // 2. モデル（矩形ポリゴン）の生成
    CreateModel();

    // 3. インスタンシング用リソースの生成
    // GPUに送るための巨大なバッファを作る
    instancingResource_ = dxCommon_->CreateBufferResource(sizeof(InstancingData) * kMaxInstanceCount);

    // マッピングしておく（書き込みっぱなしにする）
    instancingResource_->Map(0, nullptr, (void**)&instancingData_);

    // SRVは不要（VBVとしてバインドするため）
    instancingBufferView_.BufferLocation = instancingResource_->GetGPUVirtualAddress();
    instancingBufferView_.SizeInBytes = sizeof(InstancingData) * kMaxInstanceCount;
    instancingBufferView_.StrideInBytes = sizeof(InstancingData);
}

void ParticleManager::Update(Camera* camera) {
    assert(camera);
    camera_ = camera;

    // 全グループのパーティクルを更新
    for (auto& groupPair : particleGroups_) {
        ParticleGroup& group = groupPair.second;

        // イテレータで走査（削除が入るため）
        for (auto it = group.particles.begin(); it != group.particles.end();) {
            Particle& p = *it;

            // 寿命経過
            p.currentTime += 1.0f / 60.0f; // 固定フレームレート仮定

            // 寿命尽きたら削除
            if (p.currentTime >= p.lifeTime) {
                it = group.particles.erase(it);
                continue;
            }

            // 移動
            p.velocity.x += p.acceleration.x;
            p.velocity.y += p.acceleration.y;
            p.velocity.z += p.acceleration.z;

            p.position.x += p.velocity.x;
            p.position.y += p.velocity.y;
            p.position.z += p.velocity.z;

            // サイズ変更（線形補間）
            float t = p.currentTime / p.lifeTime;
            p.size = p.startSize * (1.0f - t) + p.endSize * t;

            // アルファフェードアウト
            p.color.w = 1.0f - t;

            ++it;
        }
    }
}

void ParticleManager::Draw() {
    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

    // ルートシグネチャ・PSOセット
    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    commandList->SetPipelineState(pipelineState_.Get());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 共通の矩形頂点バッファをセット
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
    // インスタンシングデータのバッファをスロット1にセット
    commandList->IASetVertexBuffers(1, 1, &instancingBufferView_);

    // カメラ行列（ビルボード用）
    Matrix4x4 viewMatrix = camera_->GetViewMatrix();
    Matrix4x4 projectionMatrix = camera_->GetProjectionMatrix();
    Matrix4x4 viewProjectionMatrix = camera_->GetViewProjectionMatrix();

    // ビルボード回転行列の作成（カメラの逆回転）
    // View行列の回転成分の逆行列を作ることで、カメラと同じ向き（＝カメラの方を向く）にする
    Matrix4x4 billboardMatrix = MatrixMath::MakeIdentity4x4();
    billboardMatrix.m[0][0] = viewMatrix.m[0][0];
    billboardMatrix.m[0][1] = viewMatrix.m[1][0];
    billboardMatrix.m[0][2] = viewMatrix.m[2][0];
    billboardMatrix.m[1][0] = viewMatrix.m[0][1];
    billboardMatrix.m[1][1] = viewMatrix.m[1][1];
    billboardMatrix.m[1][2] = viewMatrix.m[2][1];
    billboardMatrix.m[2][0] = viewMatrix.m[0][2];
    billboardMatrix.m[2][1] = viewMatrix.m[1][2];
    billboardMatrix.m[2][2] = viewMatrix.m[2][2];

    // グループごとに描画
    for (auto& groupPair : particleGroups_) {
        ParticleGroup& group = groupPair.second;
        size_t particleCount = group.particles.size();

        if (particleCount == 0) continue;

        // 上限キャップ
        uint32_t drawCount = static_cast<uint32_t>(particleCount);
        if (drawCount > kMaxInstanceCount) {
            drawCount = kMaxInstanceCount;
        }

        // インスタンシングデータを書き込む
        uint32_t index = 0;
        for (const auto& particle : group.particles) {
            if (index >= drawCount) break;

            // スケール
            Matrix4x4 scaleMatrix = MatrixMath::MakeScaleMatrix({ particle.size, particle.size, particle.size });
            // 平行移動
            Matrix4x4 translateMatrix = MatrixMath::MakeTranslateMatrix(particle.position);

            // 行列合成: Scale * BillboardRotation * Translate
            Matrix4x4 worldMatrix = MatrixMath::Multiply(scaleMatrix, MatrixMath::Multiply(billboardMatrix, translateMatrix));

            // データセット
            instancingData_[index].World = worldMatrix;
            instancingData_[index].WVP = MatrixMath::Multiply(worldMatrix, viewProjectionMatrix);
            instancingData_[index].color = particle.color;

            index++;
        }

        // テクスチャセット
        D3D12_GPU_DESCRIPTOR_HANDLE srvHandle = srvManager_->GetGPUDescriptorHandle(group.textureHandle);
        commandList->SetGraphicsRootDescriptorTable(0, srvHandle);

        // インスタンシング描画実行
        // 引数: 頂点数, インスタンス数, 開始頂点, 開始インスタンス
        // インデックスバッファを使用しない場合 (頂点数6)
        commandList->DrawInstanced(6, drawCount, 0, 0);
    }
}

void ParticleManager::CreateParticleGroup(const std::string& name, uint32_t textureHandle) {
    if (particleGroups_.contains(name)) {
        return; // すでに登録済み
    }
    ParticleGroup group;
    group.name = name;
    group.textureHandle = textureHandle;
    particleGroups_[name] = group;
}

void ParticleManager::Emit(const std::string& name, const Vector3& position, uint32_t count) {
    if (!particleGroups_.contains(name)) return;

    ParticleGroup& group = particleGroups_[name];

    std::random_device seed_gen;
    std::mt19937_64 engine(seed_gen());
    std::uniform_real_distribution<float> distPos(-1.0f, 1.0f);
    std::uniform_real_distribution<float> distVel(-0.1f, 0.1f);
    std::uniform_real_distribution<float> distColor(0.5f, 1.0f);

    for (uint32_t i = 0; i < count; ++i) {
        Particle p;
        p.position = position;
        // ランダムに少し散らす
        p.position.x += distPos(engine);
        p.position.y += distPos(engine);
        p.position.z += distPos(engine);

        p.velocity = { distVel(engine), distVel(engine), distVel(engine) };
        p.acceleration = { 0.0f, 0.0f, 0.0f }; // 重力なし

        p.color = { distColor(engine), distColor(engine), distColor(engine), 1.0f };
        p.lifeTime = 2.0f; // 2秒
        p.currentTime = 0.0f;
        p.startSize = 1.0f;
        p.endSize = 0.0f;
        p.size = p.startSize;

        group.particles.push_back(p);
    }
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
    // 矩形の頂点データ
    // インデックスバッファ不要の頂点6つ版
    VertexData vertices6[] = {
        // 三角形1
        {{ -0.5f, -0.5f, 0.0f, 1.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, -1.0f }}, // 左下
        {{  0.0f,  0.5f, 0.0f, 1.0f }, { 0.5f, 0.0f }, { 0.0f, 0.0f, -1.0f }}, // 上
        {{  0.5f, -0.5f, 0.0f, 1.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, -1.0f }}, // 右下
        // 三角形2
        {{  0.0f,  0.5f, 0.0f, 1.0f }, { 0.5f, 0.0f }, { 0.0f, 0.0f, -1.0f }}, // 上
        {{  0.5f,  0.5f, 0.0f, 1.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f, -1.0f }}, // 右上
        {{  0.5f, -0.5f, 0.0f, 1.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, -1.0f }}, // 右下
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