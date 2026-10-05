#pragma once
#include "title/TitleSkyRenderer.h"
#include "farm/render/FarmMeshLayout.h"
#include "3d/Camera.h"
#include "3d/LightingSystem.h"
#include <array>
#include <memory>
#include <vector>

class Object3d;
class Object3dCommon;
class ModelManager;
class Model;

namespace title {
class TitleFarmRenderer final {
public:
    TitleFarmRenderer();
    ~TitleFarmRenderer();
    bool Initialize(Object3dCommon* common, ModelManager* models, TextureManager* textures,
        const farm::FarmGrid& farm);
    void Finalize() noexcept;
    void Update(const Frame& frame);
    void Draw();
    [[nodiscard]] std::size_t GetObjectCount() const noexcept { return objects_.size(); }
private:
    static constexpr std::size_t kMaximumParts = 720;
    static constexpr std::size_t kGlintCount = 8;
    struct CropInstance {
        std::size_t tileIndex = 0;
        farm::FarmTileVisualData visual{};
        std::array<std::size_t, 2> partIndices{};
        std::size_t count = 0;
    };
    Object3dCommon* common_ = nullptr;
    LightingSystem* lights_ = nullptr;
    LightingSystem::DirectionalLight previousDirectional_{};
    LightingSystem::SpotLight previousSpot_{};
    float previousShadowStrength_ = 0;
    bool ready_ = false;
    Camera camera_;
    TitleSkyRenderer sky_;
    // Models/textures are manager-owned; instances and upload buffers are scene-owned.
    std::array<Model*,11> models_{};
    std::vector<farm::FarmMeshPart> parts_;
    std::vector<std::unique_ptr<Object3d>> objects_;
    std::vector<unsigned char> visible_;
    std::vector<CropInstance> crops_;
    std::unique_ptr<Object3d> landscape_;
    std::array<std::unique_ptr<Object3d>,kGlintCount> glints_;
    std::array<Vector3,4> waterCenters_{};
};
}
