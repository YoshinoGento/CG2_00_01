#pragma once
#include "title/TitlePondSystem.h"
#include <d3d12.h>
#include <wrl.h>
#include <cstdint>

class DirectXCommon;
class SrvManager;
class TextureManager;
namespace title {
// Borrows backend; owns snapshot/SRV until title teardown at the previous-frame Fence boundary.
class TitlePondRenderer final {
public:
    TitlePondRenderer() = default;
    ~TitlePondRenderer() { Finalize(); }
    TitlePondRenderer(const TitlePondRenderer&) = delete;
    TitlePondRenderer& operator=(const TitlePondRenderer&) = delete;
    bool Initialize(DirectXCommon* dx, SrvManager* srv, TextureManager* textures);
    void Finalize() noexcept;
    bool Draw(const PondFrame& frame, std::uint32_t reflectionSrv = UINT32_MAX);
private:
    DirectXCommon* dx_ = nullptr;
    SrvManager* srv_ = nullptr;
    D3D12_GPU_DESCRIPTOR_HANDLE logoSrv_{}; // Borrowed from the longer-lived TextureManager.
    Microsoft::WRL::ComPtr<ID3D12RootSignature> root_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline_;
    Microsoft::WRL::ComPtr<ID3D12Resource> snapshot_;
    std::uint32_t snapshotSrv_ = UINT32_MAX;
    D3D12_RESOURCE_STATES snapshotState_ = D3D12_RESOURCE_STATE_COPY_DEST;
};
}
