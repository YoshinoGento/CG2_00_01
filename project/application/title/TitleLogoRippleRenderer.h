#pragma once
#include "title/TitlePresentationSystem.h"
#include "2d/TextureManager.h"
#include <d3d12.h>
#include <wrl.h>

class DirectXCommon;
namespace title {
class TitleLogoRippleRenderer final {
public:
    bool Initialize(DirectXCommon* dx, TextureManager* textures);
    void Draw(const Frame& frame);
private:
    struct Constants {
        std::array<Vector4,kLogoRippleCount> ripples{};
        Vector4 style{}, tint{};
    };
    static_assert(kLogoRippleCount == 2 && sizeof(Constants) == 64);
    static_assert(sizeof(Constants) <= D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
    bool CreatePipeline();
    DirectXCommon* dx_ = nullptr;
    TextureManager* textures_ = nullptr;
    Texture2DHandle mask_{};
    Microsoft::WRL::ComPtr<ID3D12Resource> vertices_, indices_, constants_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> root_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline_;
    D3D12_VERTEX_BUFFER_VIEW vb_{};
    D3D12_INDEX_BUFFER_VIEW ib_{};
    Constants* mapped_ = nullptr;
};
}
