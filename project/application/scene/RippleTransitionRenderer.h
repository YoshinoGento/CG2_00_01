#pragma once
#include "SceneTransitionSystem.h"
#include <d3d12.h>
#include <wrl.h>
#include <limits>
#include <cstdint>

class DirectXCommon;
class SrvManager;
namespace scene {
class RippleTransitionRenderer final {
public:
    RippleTransitionRenderer() = default;
    RippleTransitionRenderer(const RippleTransitionRenderer&) = delete;
    RippleTransitionRenderer& operator=(const RippleTransitionRenderer&) = delete;
    ~RippleTransitionRenderer();
    bool Initialize(DirectXCommon* dx, SrvManager* srv);
    bool Draw(const TransitionFrame& frame);
    [[nodiscard]] bool IsReady() const noexcept { return dx_ && srv_ && root_ && pipeline_ && snapshot_ && outgoing_; }
private:
    // Backend outlives SceneManager; PostDraw must finish GPU work before destruction.
    DirectXCommon* dx_ = nullptr;
    SrvManager* srv_ = nullptr;
    uint32_t snapshotSrv_ = (std::numeric_limits<uint32_t>::max)();
    D3D12_RESOURCE_STATES snapshotState_ = D3D12_RESOURCE_STATE_COPY_DEST;
    Microsoft::WRL::ComPtr<ID3D12Resource> snapshot_;
    // Owned by this renderer, not the outgoing scene's texture cache.
    Microsoft::WRL::ComPtr<ID3D12Resource> outgoing_;
    uint32_t outgoingSrv_ = (std::numeric_limits<uint32_t>::max)();
    D3D12_RESOURCE_STATES outgoingState_ = D3D12_RESOURCE_STATE_COPY_DEST;
    bool outgoingValid_ = false;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> root_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline_;
};
}
