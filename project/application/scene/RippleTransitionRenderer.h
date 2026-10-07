#pragma once
#include "SceneTransitionSystem.h"
#include <d3d12.h>
#include <wrl.h>

class DirectXCommon;
namespace scene {
class RippleTransitionRenderer final {
public:
    bool Initialize(DirectXCommon* dx);
    void Draw(TransitionFrame frame) const;
    [[nodiscard]] bool IsReady() const noexcept { return dx_ && root_ && pipeline_; }
private:
    // Borrowed backend outlives SceneManager; no scene texture/descriptor is retained.
    DirectXCommon* dx_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> root_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline_;
};
}
