#pragma once
#include "title/TitlePresentationSystem.h"
#include "scene/SceneTransitionSystem.h"
#include "math/Matrix.h"
#include <optional>
#include <string>

namespace title {
struct PondLayout {
    Vector3 center{};
    Vector2 radii{};
    float shoreWidth = 0;
    float reflectionLine = 0;
    std::array<Vector3,3> outfall{};
    float outfallHalfWidth = 0;
};
struct PondFrame {
    std::array<float,9> screenToPond{};
    Vector4 waterColor{};
    float seconds = 0;
    float nightAmount = 0;
    float reflectionLine = 0;
    Vector4 geometry{};
    Vector4 inflow{};
    Vector4 sunlight{};
    Vector4 camera{};
};

// Owns title-only input mapping; it never accesses the playable farm or documents.
class TitlePondSystem final {
public:
    bool Initialize(const std::string& path = "Resources/title/pond_layout.json");
    void Update(float delta, const Frame& frame) noexcept;
    bool RequestStart(std::optional<Vector2> pointer = {}) noexcept;
    [[nodiscard]] const PondFrame& GetFrame() const noexcept { return frame_; }
    [[nodiscard]] const scene::PondRipplePath& GetStartPath() const noexcept { return path_; }
    [[nodiscard]] const PondLayout& GetLayout() const noexcept { return layout_; }
    [[nodiscard]] std::optional<Vector2> MapToPond(Vector2 uv) const noexcept;
    [[nodiscard]] Vector2 ProjectPondPoint(Vector2 point) const noexcept;
private:
    bool ready_ = false, requested_ = false;
    PondLayout layout_{};
    PondFrame frame_{};
    Matrix4x4 viewProjection_{};
    scene::PondRipplePath path_{};
};
}
