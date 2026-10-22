#pragma once
#include "math/Struct.h"
#include <algorithm>
#include <cmath>

namespace farm {
// Value policy only. Preserve the existing exposure compensation; tint the sky fill independently.
inline Vector4 BuildOutdoorAmbientTint(Vector4 skyTop, Vector4 skyHorizon) noexcept {
    const auto channel = [](float top, float horizon) {
        if (!std::isfinite(top) || !std::isfinite(horizon)) return 1.0f;
        constexpr float kSkyWeight = .35f, kColorAmount = .25f;
        const float sky = std::clamp(top, 0.0f, 1.0f)*kSkyWeight +
            std::clamp(horizon, 0.0f, 1.0f)*(1.0f-kSkyWeight);
        return (1.0f-kColorAmount)+kColorAmount*sky;
    };
    return {channel(skyTop.x,skyHorizon.x), channel(skyTop.y,skyHorizon.y),
        channel(skyTop.z,skyHorizon.z), 1.0f};
}
}
