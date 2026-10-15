#include "title/TitlePondRenderer.h"
#include "base/DirectXCommon.h"
#include "base/SrvManager.h"
#include "2d/TextureManager.h"
#include <array>
#include <cmath>

namespace title {
namespace {
constexpr UINT kConstantCount = 36;
static_assert(kConstantCount+3 <= 64);
}
bool TitlePondRenderer::Initialize(DirectXCommon* dx, SrvManager* srv, TextureManager* textures) {
    if (dx_ || !dx || !dx->GetDevice() || !dx->GetRenderTextureResource() || !srv || !textures) return false;
    dx_ = dx; srv_ = srv;
    const auto logo = textures->LoadTexture2D("Resources/title/logo.png");
    if (!logo.IsValid() || logo == textures->GetFallback2D()) { Finalize(); return false; }
    logoSrv_ = textures->GetGpuHandle(logo);
    const auto vs = dx->CompileShader(L"Resources/shader/RippleTransition.VS.hlsl",L"vs_6_0");
    const auto ps = dx->CompileShader(L"Resources/shader/TitlePond.PS.hlsl",L"ps_6_0");
    if (!vs || !ps) { Finalize(); return false; }
    D3D12_DESCRIPTOR_RANGE range{};
    range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; range.NumDescriptors = 1;
    std::array<D3D12_ROOT_PARAMETER,4> parameters{};
    parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    parameters[0].Constants.Num32BitValues = kConstantCount;
    parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters[1].DescriptorTable = {1,&range};
    parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    auto reflectionRange = range; reflectionRange.BaseShaderRegister = 1;
    parameters[2] = parameters[1]; parameters[2].DescriptorTable = {1,&reflectionRange};
    auto logoRange = range; logoRange.BaseShaderRegister = 2;
    parameters[3] = parameters[1]; parameters[3].DescriptorTable = {1,&logoRange};
    D3D12_STATIC_SAMPLER_DESC sampler{};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.MaxAnisotropy = 1;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    sampler.MaxLOD = D3D12_FLOAT32_MAX; sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_ROOT_SIGNATURE_DESC signature{};
    signature.NumParameters = static_cast<UINT>(parameters.size()); signature.pParameters = parameters.data();
    signature.NumStaticSamplers = 1; signature.pStaticSamplers = &sampler;
    Microsoft::WRL::ComPtr<ID3DBlob> blob,error;
    if (FAILED(D3D12SerializeRootSignature(&signature,D3D_ROOT_SIGNATURE_VERSION_1,&blob,&error))) { Finalize(); return false; }
    if (FAILED(dx->GetDevice()->CreateRootSignature(0,blob->GetBufferPointer(),blob->GetBufferSize(),IID_PPV_ARGS(&root_)))) {
        Finalize(); return false;
    }
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
    pso.pRootSignature = root_.Get();
    pso.VS = {vs->GetBufferPointer(),vs->GetBufferSize()};
    pso.PS = {ps->GetBufferPointer(),ps->GetBufferSize()};
    pso.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    pso.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; pso.RasterizerState.DepthClipEnable = TRUE;
    pso.DepthStencilState.DepthEnable = FALSE; pso.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    pso.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    pso.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    pso.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM; pso.NumRenderTargets = 1;
    pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso.SampleDesc.Count = 1; pso.SampleMask = UINT32_MAX;
    if (FAILED(dx->GetDevice()->CreateGraphicsPipelineState(&pso,IID_PPV_ARGS(&pipeline_)))) { Finalize(); return false; }
    auto description = dx->GetRenderTextureResource()->GetDesc();
    description.Flags = D3D12_RESOURCE_FLAG_NONE;
    if (description.Format != pso.RTVFormats[0] || description.SampleDesc.Count != 1) { Finalize(); return false; }
    D3D12_HEAP_PROPERTIES heap{}; heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    if (FAILED(dx->GetDevice()->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&description,
        snapshotState_,nullptr,IID_PPV_ARGS(&snapshot_)))) { Finalize(); return false; }
    snapshot_->SetName(L"Title pond mirrored scene snapshot");
    if (srv->GetAllocatedCount() >= SrvManager::kMaxSRVCount) { Finalize(); return false; }
    snapshotSrv_ = srv->Allocate();
    if (snapshotSrv_ == SrvManager::kInvalidIndex) { Finalize(); return false; }
    srv->CreateSRVforTexture2D(snapshotSrv_,snapshot_.Get(),description.Format,1);
    return true;
}
void TitlePondRenderer::Finalize() noexcept {
    if (srv_ && srv_->IsAllocated(snapshotSrv_)) srv_->Release(snapshotSrv_);
    snapshotSrv_ = UINT32_MAX;
    logoSrv_ = {};
    snapshot_.Reset(); pipeline_.Reset(); root_.Reset();
    dx_ = nullptr; srv_ = nullptr; snapshotState_ = D3D12_RESOURCE_STATE_COPY_DEST;
}
bool TitlePondRenderer::Draw(const PondFrame& frame, std::uint32_t reflectionSrv) {
    if (!dx_ || !srv_ || !snapshot_ || !root_ || !pipeline_) return false;
    auto* commands = dx_->GetCommandList();
    auto* source = dx_->GetRenderTextureResource();
    if (!commands || !source) return false;
    const auto description = source->GetDesc(), snapshotDescription = snapshot_->GetDesc();
    if (description.Width != snapshotDescription.Width || description.Height != snapshotDescription.Height ||
        description.Format != snapshotDescription.Format) return false;
    std::array<float,kConstantCount> values{};
    for (std::size_t row=0; row<3; ++row)
        for (std::size_t column=0; column<3; ++column) values[row*4+column] = frame.screenToPond[row*3+column];
    values[12] = frame.reflectionLine; values[13] = frame.seconds;
    values[14] = frame.nightAmount;
    const bool reflected = reflectionSrv != UINT32_MAX && srv_->IsAllocated(reflectionSrv);
    values[15] = reflected ? 1.0f : 0.0f;
    values[16] = frame.waterColor.x; values[17] = frame.waterColor.y; values[18] = frame.waterColor.z; values[19] = 1;
    const std::array<Vector4,4> extras{frame.geometry,frame.inflow,frame.sunlight,frame.camera};
    for (std::size_t i=0; i<extras.size(); ++i) {
        values[20+i*4] = extras[i].x; values[21+i*4] = extras[i].y;
        values[22+i*4] = extras[i].z; values[23+i*4] = extras[i].w;
    }
    for (float value : values) if (!std::isfinite(value)) return false;
    commands->OMSetRenderTargets(0,nullptr,FALSE,nullptr);
    dx_->TransitionRenderTexture(D3D12_RESOURCE_STATE_COPY_SOURCE);
    const auto transition = [&](D3D12_RESOURCE_STATES after) {
        if (snapshotState_ == after) return;
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition = {snapshot_.Get(),D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,snapshotState_,after};
        commands->ResourceBarrier(1,&barrier); snapshotState_ = after;
    };
    transition(D3D12_RESOURCE_STATE_COPY_DEST);
    commands->CopyResource(snapshot_.Get(),source);
    dx_->TransitionRenderTexture(D3D12_RESOURCE_STATE_RENDER_TARGET);
    transition(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    dx_->SetSceneRenderTarget();
    const D3D12_VIEWPORT viewport{0,0,static_cast<float>(description.Width),static_cast<float>(description.Height),0,1};
    const D3D12_RECT scissor{0,0,static_cast<LONG>(description.Width),static_cast<LONG>(description.Height)};
    commands->RSSetViewports(1,&viewport); commands->RSSetScissorRects(1,&scissor);
    srv_->PreDraw(); commands->SetGraphicsRootSignature(root_.Get()); commands->SetPipelineState(pipeline_.Get());
    commands->SetGraphicsRoot32BitConstants(0,kConstantCount,values.data(),0);
    commands->SetGraphicsRootDescriptorTable(1,srv_->GetGPUDescriptorHandle(snapshotSrv_));
    commands->SetGraphicsRootDescriptorTable(2,srv_->GetGPUDescriptorHandle(reflected ? reflectionSrv : snapshotSrv_));
    commands->SetGraphicsRootDescriptorTable(3,logoSrv_);
    commands->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commands->DrawInstanced(3,1,0,0);
    return true;
}
}
