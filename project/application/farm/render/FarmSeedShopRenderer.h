#pragma once
#include "3d/Object3d.h"
#include "3d/Camera.h"
#include <array>

class ModelManager;

// Scene-owned presentation, with an independent camera. No farm or money mutation.
class FarmSeedShopRenderer final {
public:
    bool Initialize(Object3dCommon* common, ModelManager* models, Texture2DHandle white);
    void Draw(int selected);
    [[nodiscard]] bool IsReady() const noexcept { return ready_; }
private:
    static constexpr std::size_t kPartCount = 17;
    std::array<Object3d, kPartCount> objects_{};
    Camera camera_;
    bool ready_ = false;
};
