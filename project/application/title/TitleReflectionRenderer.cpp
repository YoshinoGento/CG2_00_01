#include "title/TitleReflectionRenderer.h"
#include "farm/render/FarmOutdoorLighting.h"
#include "base/DirectXCommon.h"
#include "base/SrvManager.h"
#include "3d/Model.h"
#include <array>
#include <cmath>
#include <cstring>

namespace title {
bool TitleReflectionRenderer::Initialize(DirectXCommon* dx, SrvManager* srv, TextureManager* textures) {
    if (dx_ || !dx || !dx->GetDevice() || !srv || !textures) return false;
    dx_ = dx; srv_ = srv; textures_ = textures;
    const auto fail = [&] { Finalize(); return false; };
    auto* device = dx->GetDevice();
    D3D12_DESCRIPTOR_HEAP_DESC heapDescription{};
    heapDescription.NumDescriptors = 1; heapDescription.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    if (FAILED(device->CreateDescriptorHeap(&heapDescription,IID_PPV_ARGS(&rtv_)))) return fail();
    heapDescription.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    if (FAILED(device->CreateDescriptorHeap(&heapDescription,IID_PPV_ARGS(&dsv_)))) return fail();
    D3D12_RESOURCE_DESC description{};
    description.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    description.Width = kWidth; description.Height = kHeight;
    description.DepthOrArraySize = 1; description.MipLevels = 1; description.SampleDesc.Count = 1;
    description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    description.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    D3D12_HEAP_PROPERTIES heap{}; heap.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_CLEAR_VALUE clear{}; clear.Format = description.Format; clear.Color[3] = 1;
    if (FAILED(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&description,state_,
        &clear,IID_PPV_ARGS(&color_)))) return fail();
    color_->SetName(L"Title planar reflection 640x360");
    device->CreateRenderTargetView(color_.Get(),nullptr,rtv_->GetCPUDescriptorHandleForHeapStart());
    description.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    description.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    clear = {}; clear.Format = description.Format; clear.DepthStencil.Depth = 1;
    if (FAILED(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&description,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,&clear,IID_PPV_ARGS(&depth_)))) return fail();
    device->CreateDepthStencilView(depth_.Get(),nullptr,dsv_->GetCPUDescriptorHandleForHeapStart());
    if (srv->GetAllocatedCount() >= SrvManager::kMaxSRVCount) return fail();
    srvIndex_ = srv->Allocate();
    if (srvIndex_ == SrvManager::kInvalidIndex) return fail();
    srv->CreateSRVforTexture2D(srvIndex_,color_.Get(),DXGI_FORMAT_R8G8B8A8_UNORM,1);
    if (!CreatePipeline() || !sky_.Initialize(dx,textures)) return fail();
    return true;
}

bool TitleReflectionRenderer::CreatePipeline() {
    const auto vs = dx_->CompileShader(L"Resources/shader/TitleReflection.VS.hlsl",L"vs_6_0");
    const auto ps = dx_->CompileShader(L"Resources/shader/TitleReflection.PS.hlsl",L"ps_6_0");
    if (!vs || !ps) return false;
    D3D12_DESCRIPTOR_RANGE range{}; range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; range.NumDescriptors = 1;
    std::array<D3D12_ROOT_PARAMETER,2> parameters{};
    parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    parameters[0].Constants.Num32BitValues = kRootConstants;
    parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters[1].DescriptorTable = {1,&range}; parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_STATIC_SAMPLER_DESC sampler{};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.MaxAnisotropy = 1; sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER; sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_ROOT_SIGNATURE_DESC signature{};
    signature.NumParameters = static_cast<UINT>(parameters.size()); signature.pParameters = parameters.data();
    signature.NumStaticSamplers = 1; signature.pStaticSamplers = &sampler;
    signature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    Microsoft::WRL::ComPtr<ID3DBlob> blob,error;
    if (FAILED(D3D12SerializeRootSignature(&signature,D3D_ROOT_SIGNATURE_VERSION_1,&blob,&error)) ||
        FAILED(dx_->GetDevice()->CreateRootSignature(0,blob->GetBufferPointer(),blob->GetBufferSize(),IID_PPV_ARGS(&root_)))) return false;
    const std::array<D3D12_INPUT_ELEMENT_DESC,3> inputs{{
        {"POSITION",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
        {"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,16,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
        {"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,28,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0}}};
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
    pso.pRootSignature = root_.Get();
    pso.VS = {vs->GetBufferPointer(),vs->GetBufferSize()}; pso.PS = {ps->GetBufferPointer(),ps->GetBufferSize()};
    pso.InputLayout = {inputs.data(),static_cast<UINT>(inputs.size())};
    pso.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    // Mirroring reverses winding; thin foliage is two-sided in this lower-resolution pass.
    pso.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID; pso.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    pso.RasterizerState.DepthClipEnable = TRUE;
    pso.DepthStencilState.DepthEnable = TRUE; pso.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    pso.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    pso.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT; pso.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    pso.NumRenderTargets = 1; pso.SampleDesc.Count = 1; pso.SampleMask = UINT32_MAX;
    pso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    return SUCCEEDED(dx_->GetDevice()->CreateGraphicsPipelineState(&pso,IID_PPV_ARGS(&pipeline_)));
}

void TitleReflectionRenderer::Update(const Frame& frame, const Camera& camera, float waterHeight,
    const LightingSystem::DirectionalLight& light) noexcept {
    if (!pipeline_ || !std::isfinite(waterHeight)) return;
    viewProjection_ = camera.GetViewProjectionMatrix(); light_ = light; waterHeight_ = waterHeight;
    outdoorAmbient_ = farm::BuildOutdoorAmbientTint(frame.skyTop,frame.skyHorizon);
    sky_.Update(frame,camera,true);
}
void TitleReflectionRenderer::Transition(D3D12_RESOURCE_STATES after) {
    if (state_ == after) return;
    D3D12_RESOURCE_BARRIER barrier{}; barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition = {color_.Get(),D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,state_,after};
    dx_->GetCommandList()->ResourceBarrier(1,&barrier); state_ = after;
}
bool TitleReflectionRenderer::Begin() {
    if (!pipeline_ || !color_ || !dx_->GetCommandList() || active_) return false;
    active_ = true;
    Transition(D3D12_RESOURCE_STATE_RENDER_TARGET);
    auto* commands = dx_->GetCommandList();
    const auto rtv = rtv_->GetCPUDescriptorHandleForHeapStart(), dsv = dsv_->GetCPUDescriptorHandleForHeapStart();
    commands->OMSetRenderTargets(1,&rtv,FALSE,&dsv);
    const float clear[4]{0,0,0,1}; commands->ClearRenderTargetView(rtv,clear,0,nullptr);
    commands->ClearDepthStencilView(dsv,D3D12_CLEAR_FLAG_DEPTH,1,0,0,nullptr);
    const D3D12_VIEWPORT viewport{0,0,float(kWidth),float(kHeight),0,1};
    const D3D12_RECT scissor{0,0,LONG(kWidth),LONG(kHeight)};
    commands->RSSetViewports(1,&viewport); commands->RSSetScissorRects(1,&scissor);
    srv_->PreDraw(); sky_.Draw();
    return true;
}
void TitleReflectionRenderer::Draw(Model* model, const Matrix4x4& world, Texture2DHandle texture, Vector4 color, bool lit) {
    if (!active_ || !model || model->HasSkinCluster() || !texture.IsValid()) return;
    std::array<float,kRootConstants> values{};
    std::memcpy(values.data(),&world,sizeof(world));
    std::memcpy(values.data()+16,&viewProjection_,sizeof(viewProjection_));
    std::memcpy(values.data()+32,&color,sizeof(color));
    values[36] = light_.direction.x; values[37] = light_.direction.y; values[38] = light_.direction.z; values[39] = light_.intensity;
    std::memcpy(values.data()+40,&light_.color,sizeof(light_.color));
    values[44] = waterHeight_; values[45] = lit ? 1.0f : 0.0f;
    std::memcpy(values.data()+48,&outdoorAmbient_,sizeof(outdoorAmbient_));
    for (float value : values) if (!std::isfinite(value)) return;
    auto* commands = dx_->GetCommandList();
    commands->SetGraphicsRootSignature(root_.Get()); commands->SetPipelineState(pipeline_.Get());
    commands->SetGraphicsRoot32BitConstants(0,kRootConstants,values.data(),0);
    commands->SetGraphicsRootDescriptorTable(1,textures_->GetGpuHandle(texture));
    commands->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    model->Draw(commands,1);
}
void TitleReflectionRenderer::End() {
    if (!active_) return;
    auto* commands = dx_->GetCommandList();
    commands->OMSetRenderTargets(0,nullptr,FALSE,nullptr);
    Transition(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    dx_->SetSceneRenderTarget();
    const auto description = dx_->GetRenderTextureResource()->GetDesc();
    const D3D12_VIEWPORT viewport{0,0,float(description.Width),float(description.Height),0,1};
    const D3D12_RECT scissor{0,0,LONG(description.Width),LONG(description.Height)};
    commands->RSSetViewports(1,&viewport); commands->RSSetScissorRects(1,&scissor); active_ = false;
}
void TitleReflectionRenderer::Finalize() noexcept {
    sky_.Finalize();
    if (srv_ && srv_->IsAllocated(srvIndex_)) srv_->Release(srvIndex_);
    srvIndex_ = UINT32_MAX; active_ = false;
    color_.Reset(); depth_.Reset(); rtv_.Reset(); dsv_.Reset(); root_.Reset(); pipeline_.Reset();
    dx_ = nullptr; srv_ = nullptr; textures_ = nullptr; state_ = D3D12_RESOURCE_STATE_RENDER_TARGET;
}
}
