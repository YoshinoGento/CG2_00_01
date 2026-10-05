#pragma once

#include "math/Struct.h"
#include <cmath>

namespace title {
inline constexpr float kWidth = 1280.0f;
inline constexpr float kHeight = 720.0f;
inline constexpr Vector3 kCameraPosition{6.5f, 4.6f, -7.5f};
inline constexpr Vector3 kCameraRotation{.16f, -.45f, 0};
inline constexpr float kCameraFovY = .72f;
inline constexpr Vector2 kStartCenter{640, 650};
inline constexpr Vector2 kStartSize{280, 80};
inline constexpr Vector2 kLogoCenter{300, 152};
inline constexpr Vector2 kLogoSize{456, 168};

// Input and drawing share virtual pixels, regardless of client size or editor letterboxing.
inline bool HitTestStartButton(Vector2 position) noexcept {
    return std::isfinite(position.x) && std::isfinite(position.y) &&
        std::abs(position.x - kStartCenter.x) <= kStartSize.x * .5f &&
        std::abs(position.y - kStartCenter.y) <= kStartSize.y * .5f;
}

inline bool HitTestStartButtonInViewport(Vector2 position, Vector2 virtualResolution) noexcept {
    if (!std::isfinite(virtualResolution.x) || !std::isfinite(virtualResolution.y) ||
        virtualResolution.x <= 0 || virtualResolution.y <= 0) return false;
    return HitTestStartButton({position.x*kWidth/virtualResolution.x,
        position.y*kHeight/virtualResolution.y});
}
}
