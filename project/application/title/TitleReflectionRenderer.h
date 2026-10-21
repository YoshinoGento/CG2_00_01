#pragma once
#include "title/TitleSkyRenderer.h"
#include "3d/Camera.h"
#include "3d/LightingSystem.h"
#include <cstdint>

class Model;
class SrvManager;
namespace title {
// Owns one bounded reflection target; borrows models/textures and the backend.
// Root constants capture each draw's data without overwriting main-view upload buffers.
class TitleReflectionRenderer final {
public:
    ~TitleReflectionRenderer() { Finalize(); }
    TitleReflectionRenderer() = default;
    TitleReflectionRenderer(const TitleReflectionRenderer&) = delete;
    TitleReflectionRenderer& operator=(const TitleReflectionRenderer&) = delete;
    bool Initialize(DirectXCommon* dx, SrvManager* srv, TextureManager* textures);
    void Finalize() noexcept;
    void Update(const Frame& frame, const Camera& camera, float waterHeight,
        const LightingSystem::DirectionalLight& light) noexcept;
    bool Begin();
    void Draw(Model* model, const Matrix4x4& world, Texture2DHandle texture,
        Vector4 color, bool lit = true);
    void End();
    [[nodiscard]] std::uint32_t GetSrv() const noexcept { return srvIndex_; }
private:
    static constexpr UINT kWidth = 640, kHeight = 360, kRootConstants = 52;
    static_assert(kRootConstants+1 <= 64);
    bool CreatePipeline();
    void Transition(D3D12_RESOURCE_STATES after);
    DirectXCommon* dx_ = nullptr;
    SrvManager* srv_ = nullptr;
    TextureManager* textures_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> color_, depth_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtv_, dsv_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> root_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline_;
    TitleSkyRenderer sky_;
    Matrix4x4 viewProjection_{};
    LightingSystem::DirectionalLight light_{};
    Vector4 outdoorAmbient_{};
    float waterHeight_ = 0;
    std::uint32_t srvIndex_ = UINT32_MAX;
    D3D12_RESOURCE_STATES state_ = D3D12_RESOURCE_STATE_RENDER_TARGET;
    bool active_ = false;
};
}
