#pragma once

#include "math/Struct.h"

#include <d3d12.h>
#include <wrl.h>

#include <cstdint>
#include <string>

class DirectXCommon;
class SrvManager;

class RenderTexture {
public:
    void Initialize(
        DirectXCommon* dxCommon,
        uint32_t width,
        uint32_t height,
        DXGI_FORMAT format,
        const Vector4& clearColor,
        D3D12_RESOURCE_STATES initialState,
        const std::string& debugName);

    void CreateSRV(SrvManager* srvManager, DXGI_FORMAT srvFormat, UINT mipLevels = 1);
	void Finalize(SrvManager* srvManager);
    void Transition(ID3D12GraphicsCommandList* commandList, D3D12_RESOURCE_STATES after);
    void Clear(ID3D12GraphicsCommandList* commandList);
    void SetClearColor(const Vector4& clearColor) { clearColor_ = clearColor; }

    ID3D12Resource* GetResource() const { return resource_.Get(); }
    D3D12_CPU_DESCRIPTOR_HANDLE GetRTV() const { return rtvHandle_; }
    D3D12_GPU_DESCRIPTOR_HANDLE GetSRV() const { return srvGpuHandle_; }
    uint32_t GetSrvIndex() const { return srvIndex_; }
    D3D12_RESOURCE_STATES GetState() const { return state_; }
    DXGI_FORMAT GetFormat() const { return format_; }
    uint32_t GetWidth() const { return width_; }
    uint32_t GetHeight() const { return height_; }

private:
    Microsoft::WRL::ComPtr<ID3D12Resource> resource_;
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle_{};
    D3D12_GPU_DESCRIPTOR_HANDLE srvGpuHandle_{};
	static constexpr uint32_t kInvalidSrvIndex = UINT32_MAX;
	uint32_t srvIndex_ = kInvalidSrvIndex;
    D3D12_RESOURCE_STATES state_ = D3D12_RESOURCE_STATE_COMMON;
    DXGI_FORMAT format_ = DXGI_FORMAT_R8G8B8A8_UNORM;
    uint32_t width_ = 0;
    uint32_t height_ = 0;
    Vector4 clearColor_{};
};
