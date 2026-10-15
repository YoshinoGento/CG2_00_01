#pragma once
#include "title/TitlePresentationSystem.h"
#include "math/Matrix.h"
#include <d3d12.h>
#include <wrl.h>

class Object3dCommon;
class ModelManager;
class Model;
class Camera;

namespace title {
class TitleGrassRenderer final {
public:
    TitleGrassRenderer() = default;
    ~TitleGrassRenderer() { Finalize(); }
    TitleGrassRenderer(const TitleGrassRenderer&) = delete;
    TitleGrassRenderer& operator=(const TitleGrassRenderer&) = delete;
    bool Initialize(Object3dCommon* common, ModelManager* models);
    void Update(const Frame& frame, const Camera& camera) noexcept;
    void DrawShadow();
    void Draw();
    void DrawReflection(float waterHeight);
    void Finalize() noexcept;
private:
    static constexpr std::size_t kMaximumPatches = 1024;
    struct Instance {
        Vector4 positionScale{};
        Vector4 tint{};
        Vector2 yaw{};
        Vector2 padding{};
    };
    struct Constants {
        Matrix4x4 viewProjection{};
        Vector4 lightColor{};
        Vector4 lightDirectionIntensity{};
        Vector4 wind{};
        Vector4 wave{};
    };
    static_assert(sizeof(Instance) == 48);
    static_assert(sizeof(Constants) == 128);
    static_assert(sizeof(Constants) <= D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
    bool CreatePipelines();
    void BindAndDraw(bool shadow);
    // Backend/Model/shadow SRV are borrowed; retained buffers outlive both passes until PostDraw Fence.
    Object3dCommon* common_ = nullptr;
    Model* model_ = nullptr;
    Model* farModel_ = nullptr;
    UINT count_ = 0;
    UINT shadowCount_ = 0;
    bool ready_ = false;
    D3D12_VERTEX_BUFFER_VIEW instancesView_{};
    Microsoft::WRL::ComPtr<ID3D12Resource> instances_, mainConstants_, shadowConstants_, reflectionConstants_;
    Constants* mainMapped_ = nullptr;
    Constants* shadowMapped_ = nullptr;
    Constants* reflectionMapped_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> root_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> mainPipeline_, shadowPipeline_, reflectionPipeline_;
};
}
