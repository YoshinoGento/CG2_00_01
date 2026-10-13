#include "RippleTransitionRenderer.h"
#include "base/DirectXCommon.h"
#include "base/SrvManager.h"
#include <array>

namespace scene {
namespace {
constexpr std::size_t kShaderConstantCount = 8+kMaximumWaterDrops*4+4;
constexpr float kWavePixelsPerSecond = 520;
constexpr float kRefractionPixels = 9;
constexpr float kBlurPixels = 2.2f;
static_assert(kMaximumWaterDrops == 4, "Must match the Shader drop array.");
static_assert(kShaderConstantCount+1 <= 64, "Root signature DWORD budget.");
}
RippleTransitionRenderer::~RippleTransitionRenderer() {
    if (srv_ && srv_->IsAllocated(snapshotSrv_)) srv_->Release(snapshotSrv_);
}
bool RippleTransitionRenderer::Initialize(DirectXCommon* dx, SrvManager* srv) {
    if (dx_ || !dx || !dx->GetDevice() || !dx->GetRenderTextureResource() || !srv) return false;
    dx_ = dx;
    srv_ = srv;
    const auto vs = dx_->CompileShader(L"Resources/shader/RippleTransition.VS.hlsl",L"vs_6_0");
    const auto ps = dx_->CompileShader(L"Resources/shader/RippleTransition.PS.hlsl",L"ps_6_0");
    if (!vs || !ps) return false;
    std::array<D3D12_ROOT_PARAMETER,2> parameters{};
    parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    parameters[0].Constants.Num32BitValues = static_cast<UINT>(kShaderConstantCount);
    parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_DESCRIPTOR_RANGE range{};
    range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    range.NumDescriptors = 1;
    parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters[1].DescriptorTable.NumDescriptorRanges = 1;
    parameters[1].DescriptorTable.pDescriptorRanges = &range;
    parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_STATIC_SAMPLER_DESC sampler{};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    sampler.MaxAnisotropy = 1;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_ROOT_SIGNATURE_DESC signature{};
    signature.NumParameters = static_cast<UINT>(parameters.size());
    signature.pParameters = parameters.data();
    signature.NumStaticSamplers = 1;
    signature.pStaticSamplers = &sampler;
    Microsoft::WRL::ComPtr<ID3DBlob> blob,error;
    if (FAILED(D3D12SerializeRootSignature(&signature,D3D_ROOT_SIGNATURE_VERSION_1,&blob,&error))) return false;
    if (FAILED(dx_->GetDevice()->CreateRootSignature(0,blob->GetBufferPointer(),blob->GetBufferSize(),IID_PPV_ARGS(&root_)))) return false;
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
    pso.pRootSignature = root_.Get();
    pso.VS = {vs->GetBufferPointer(),vs->GetBufferSize()};
    pso.PS = {ps->GetBufferPointer(),ps->GetBufferSize()};
    auto& blend = pso.BlendState.RenderTarget[0];
    blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    blend.BlendEnable = FALSE;
    blend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
    blend.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    blend.BlendOp = blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blend.SrcBlendAlpha = D3D12_BLEND_ONE;
    blend.DestBlendAlpha = D3D12_BLEND_ZERO;
    pso.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    pso.RasterizerState.DepthClipEnable = TRUE;
    pso.DepthStencilState.DepthEnable = FALSE;
    pso.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    pso.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    pso.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    pso.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    pso.NumRenderTargets = 1;
    pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso.SampleDesc.Count = 1;
    pso.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    if (FAILED(dx_->GetDevice()->CreateGraphicsPipelineState(&pso,IID_PPV_ARGS(&pipeline_)))) return false;
    auto description = dx_->GetRenderTextureResource()->GetDesc();
    description.Flags = D3D12_RESOURCE_FLAG_NONE;
    if (description.Format != pso.RTVFormats[0] || description.SampleDesc.Count != 1) return false;
    D3D12_HEAP_PROPERTIES heap{};
    heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    if (FAILED(dx_->GetDevice()->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&description,
        snapshotState_,nullptr,IID_PPV_ARGS(&snapshot_)))) return false;
    snapshot_->SetName(L"Water ripple scene snapshot");
    if (srv_->GetAllocatedCount() >= SrvManager::kMaxSRVCount) { snapshot_.Reset(); return false; }
    snapshotSrv_ = srv_->Allocate();
    if (snapshotSrv_ == SrvManager::kInvalidIndex) { snapshot_.Reset(); return false; }
    srv_->CreateSRVforTexture2D(snapshotSrv_,snapshot_.Get(),description.Format,1);
    return true;
}
bool RippleTransitionRenderer::Draw(const TransitionFrame& frame) {
    const bool transitioning = frame.phase == TransitionPhase::Covering || frame.phase == TransitionPhase::Revealing;
    const bool hasDrops = std::any_of(frame.drops.begin(),frame.drops.end(),[](const auto& drop) { return drop.active; });
    if (!IsReady() || (!transitioning && !hasDrops)) return false;
    if (!std::isfinite(frame.progress)) return false;
    auto* source = dx_->GetRenderTextureResource();
    const auto description = snapshot_->GetDesc();
    const auto sourceDescription = source->GetDesc();
    if (description.Width != sourceDescription.Width || description.Height != sourceDescription.Height ||
        description.Format != sourceDescription.Format) return false;
    std::array<float,kShaderConstantCount> values{frame.center.x,frame.center.y,
        std::clamp(frame.progress,0.0f,1.0f),frame.phase == TransitionPhase::Revealing ? 1.0f : 0.0f,
        static_cast<float>(description.Width),static_cast<float>(description.Height),kDropFallSeconds,transitioning ? 1.0f : 0.0f};
    for (std::size_t i=0; i<frame.drops.size(); ++i) {
        const auto& drop = frame.drops[i];
        values[8+i*4] = drop.center.x; values[9+i*4] = drop.center.y;
        values[10+i*4] = drop.age; values[11+i*4] = drop.active ? 1.0f : 0.0f;
    }
    constexpr std::size_t tuningOffset = 8+kMaximumWaterDrops*4;
    values[tuningOffset] = kDropLifetimeSeconds;
    values[tuningOffset+1] = kWavePixelsPerSecond;
    values[tuningOffset+2] = kRefractionPixels;
    values[tuningOffset+3] = kBlurPixels;
    auto* commands = dx_->GetCommandList();
    commands->OMSetRenderTargets(0,nullptr,FALSE,nullptr);
    dx_->TransitionRenderTexture(D3D12_RESOURCE_STATE_COPY_SOURCE);
    const auto transitionSnapshot = [&](D3D12_RESOURCE_STATES after) {
        if (snapshotState_ == after) return;
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition = {snapshot_.Get(),D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,snapshotState_,after};
        commands->ResourceBarrier(1,&barrier);
        snapshotState_ = after;
    };
    transitionSnapshot(D3D12_RESOURCE_STATE_COPY_DEST);
    commands->CopyResource(snapshot_.Get(),source);
    dx_->TransitionRenderTexture(D3D12_RESOURCE_STATE_RENDER_TARGET);
    transitionSnapshot(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    dx_->SetSceneRenderTarget();
    const D3D12_VIEWPORT viewport{0,0,values[4],values[5],0,1};
    const D3D12_RECT scissor{0,0,static_cast<LONG>(description.Width),static_cast<LONG>(description.Height)};
    commands->RSSetViewports(1,&viewport);
    commands->RSSetScissorRects(1,&scissor);
    srv_->PreDraw();
    commands->SetGraphicsRootSignature(root_.Get());
    commands->SetPipelineState(pipeline_.Get());
    // Sample the separate snapshot, never the bound output RT.
    commands->SetGraphicsRoot32BitConstants(0,static_cast<UINT>(values.size()),values.data(),0);
    commands->SetGraphicsRootDescriptorTable(1,srv_->GetGPUDescriptorHandle(snapshotSrv_));
    commands->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commands->DrawInstanced(3,1,0,0);
    return true;
}
}
