#include "title/TitleLogoRippleRenderer.h"
#include "base/DirectXCommon.h"
#include <cstring>
#include <filesystem>

namespace title {
bool TitleLogoRippleRenderer::Initialize(DirectXCommon* dx, TextureManager* textures) {
    if (!dx || !textures || !textures->IsInitialized() ||
        !std::filesystem::is_regular_file("Resources/title/logo_reflection.png")) return false;
    dx_ = dx;
    textures_ = textures;
    mask_ = textures_->LoadTexture2D("Resources/title/logo_reflection.png");
    if (!mask_.IsValid() || mask_ == textures_->GetFallback2D()) return false;
    const auto desc = textures_->GetResourceDesc(mask_);
    if (desc.Width != static_cast<UINT64>(kLogoSize.x) || desc.Height != static_cast<UINT>(kLogoSize.y)) return false;
    struct Vertex { Vector2 position, uv; };
    constexpr float left = (kLogoCenter.x-kLogoSize.x*.5f)*2/kWidth-1;
    constexpr float right = (kLogoCenter.x+kLogoSize.x*.5f)*2/kWidth-1;
    constexpr float top = 1-(kLogoCenter.y-kLogoSize.y*.5f)*2/kHeight;
    constexpr float bottom = 1-(kLogoCenter.y+kLogoSize.y*.5f)*2/kHeight;
    constexpr std::array<Vertex,4> vertices{{{{left,top},{0,0}},{{right,top},{1,0}},
        {{left,bottom},{0,1}},{{right,bottom},{1,1}}}};
    constexpr std::array<std::uint32_t,6> indices{{0,1,2,2,1,3}};
    vertices_ = dx_->CreateBufferResource(sizeof(vertices));
    indices_ = dx_->CreateBufferResource(sizeof(indices));
    constants_ = dx_->CreateBufferResource(D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
    if (!vertices_ || !indices_ || !constants_) return false;
    void* memory = nullptr;
    const D3D12_RANGE noRead{0,0};
    if (FAILED(vertices_->Map(0,&noRead,&memory)) || !memory) return false;
    std::memcpy(memory,vertices.data(),sizeof(vertices)); vertices_->Unmap(0,nullptr);
    if (FAILED(indices_->Map(0,&noRead,&memory)) || !memory) return false;
    std::memcpy(memory,indices.data(),sizeof(indices)); indices_->Unmap(0,nullptr);
    if (FAILED(constants_->Map(0,&noRead,reinterpret_cast<void**>(&mapped_))) || !mapped_) return false;
    *mapped_ = {};
    vb_ = {vertices_->GetGPUVirtualAddress(),static_cast<UINT>(sizeof(vertices)),sizeof(Vertex)};
    ib_ = {indices_->GetGPUVirtualAddress(),static_cast<UINT>(sizeof(indices)),DXGI_FORMAT_R32_UINT};
    return CreatePipeline();
}

bool TitleLogoRippleRenderer::CreatePipeline() {
    const auto vs = dx_->CompileShader(L"Resources/shader/TitleLogoRipple.VS.hlsl",L"vs_6_0");
    const auto ps = dx_->CompileShader(L"Resources/shader/TitleLogoRipple.PS.hlsl",L"ps_6_0");
    if (!vs || !ps) return false;
    D3D12_DESCRIPTOR_RANGE range{};
    range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    range.NumDescriptors = 1;
    D3D12_ROOT_PARAMETER parameters[2]{};
    parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    parameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    parameters[1].DescriptorTable = {1,&range};
    parameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_STATIC_SAMPLER_DESC sampler{};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    D3D12_ROOT_SIGNATURE_DESC signature{};
    signature.pParameters = parameters; signature.NumParameters = 2;
    signature.pStaticSamplers = &sampler; signature.NumStaticSamplers = 1;
    signature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    Microsoft::WRL::ComPtr<ID3DBlob> blob,error;
    if (FAILED(D3D12SerializeRootSignature(&signature,D3D_ROOT_SIGNATURE_VERSION_1,&blob,&error))) return false;
    if (FAILED(dx_->GetDevice()->CreateRootSignature(0,blob->GetBufferPointer(),blob->GetBufferSize(),IID_PPV_ARGS(&root_)))) return false;
    const D3D12_INPUT_ELEMENT_DESC input[] = {
        {"POSITION",0,DXGI_FORMAT_R32G32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
        {"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,8,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0}};
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso{};
    pso.pRootSignature = root_.Get();
    pso.VS = {vs->GetBufferPointer(),vs->GetBufferSize()};
    pso.PS = {ps->GetBufferPointer(),ps->GetBufferSize()};
    pso.InputLayout = {input,2};
    auto& blend = pso.BlendState.RenderTarget[0];
    blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    blend.BlendEnable = TRUE;
    blend.SrcBlend = D3D12_BLEND_SRC_ALPHA; blend.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    blend.BlendOp = blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blend.SrcBlendAlpha = D3D12_BLEND_ONE; blend.DestBlendAlpha = D3D12_BLEND_ZERO;
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
    pso.SampleDesc.Count = 1; pso.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    return SUCCEEDED(dx_->GetDevice()->CreateGraphicsPipelineState(&pso,IID_PPV_ARGS(&pipeline_)));
}

void TitleLogoRippleRenderer::Draw(const Frame& frame) {
    if (!mapped_ || !root_ || !pipeline_) return;
    bool visible = false;
    for (std::size_t i=0; i<frame.logoRipples.size(); ++i) {
        const auto& ripple = frame.logoRipples[i];
        mapped_->ripples[i] = {ripple.center.x,ripple.center.y,ripple.radius,ripple.alpha};
        visible = visible || ripple.alpha > 0;
    }
    if (!visible) return;
    mapped_->style = {kLogoRippleWidth,kLogoRippleVerticalScale,kLogoRippleEchoDistance,kLogoRippleEchoStrength};
    mapped_->tint = {.02f,.36f,.82f,1};
    // One retained CB is reused after the engine's PostDraw Fence wait, never within multiple draws.
    auto* commands = dx_->GetCommandList();
    commands->SetGraphicsRootSignature(root_.Get()); commands->SetPipelineState(pipeline_.Get());
    commands->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commands->IASetVertexBuffers(0,1,&vb_); commands->IASetIndexBuffer(&ib_);
    commands->SetGraphicsRootConstantBufferView(0,constants_->GetGPUVirtualAddress());
    commands->SetGraphicsRootDescriptorTable(1,textures_->GetGpuHandle(mask_));
    commands->DrawIndexedInstanced(6,1,0,0,0);
}
}
