#pragma once

#include <d3d12.h>
#include <wrl.h>

#include <string>

class DirectXCommon;
class FullscreenPass;

struct PostEffectPassDesc {
    std::string name;
    FullscreenPass* pass = nullptr;
    bool enabled = true;

    D3D12_GPU_VIRTUAL_ADDRESS cbv = 0;

    D3D12_GPU_DESCRIPTOR_HANDLE srvT1{};
    D3D12_GPU_DESCRIPTOR_HANDLE srvT2{};

    bool useCBV = false;
    bool useSrvT1 = false;
    bool useSrvT2 = false;
};

class FullscreenPass {
public:
    void Initialize(
        DirectXCommon* dxCommon,
        const std::wstring& pixelShaderPath,
        DXGI_FORMAT rtvFormat);

    void Draw(
        ID3D12GraphicsCommandList* commandList,
        D3D12_GPU_DESCRIPTOR_HANDLE inputSrv,
        const PostEffectPassDesc& passDesc) const;

    bool IsInitialized() const { return pipelineState_ != nullptr; }

private:
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
};
