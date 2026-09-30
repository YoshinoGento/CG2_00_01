#pragma once
#include "title/TitlePresentationSystem.h"
#include "2d/TextureManager.h"
#include "math/Matrix.h"
#include <d3d12.h>
#include <wrl.h>

class Camera;
class DirectXCommon;

namespace title {
class TitleSkyRenderer final {
public:
    bool Initialize(DirectXCommon* dx, TextureManager* textures);
    void Update(const Frame& frame, const Camera& camera) noexcept;
    void Draw() const;
private:
    struct Constants {
        Matrix4x4 wvp{};
        Vector4 top{}, horizon{}, clouds{}, sun{}, moon{}, discs{}, motion{}, animation{};
    };
    static_assert(sizeof(Constants) == 192);
    static_assert(sizeof(Constants) <= 256);
    bool CreatePipeline();
    DirectXCommon* dx_ = nullptr;
    TextureManager* textures_ = nullptr;
    Texture2DHandle texture_{};
    Microsoft::WRL::ComPtr<ID3D12Resource> vertices_, indices_, constants_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> root_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline_;
    D3D12_VERTEX_BUFFER_VIEW vb_{};
    D3D12_INDEX_BUFFER_VIEW ib_{};
    Constants* mapped_ = nullptr;
    UINT indexCount_ = 0;
};
}
