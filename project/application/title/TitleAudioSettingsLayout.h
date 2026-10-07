#pragma once
#include "math/Struct.h"
#include <array>
#include <cmath>
namespace title {
struct AudioSettingsRect {
    Vector2 position, size;
    [[nodiscard]] bool Contains(Vector2 point) const noexcept {
        return std::isfinite(point.x) && std::isfinite(point.y) &&
            point.x>=position.x && point.x<=position.x+size.x &&
            point.y>=position.y && point.y<=position.y+size.y;
    }
};
inline constexpr AudioSettingsRect kAudioSettingsButton{{1068,44},{176,48}};
inline constexpr AudioSettingsRect kAudioSettingsPanel{{320,208},{640,352}};
inline constexpr AudioSettingsRect kAudioSettingsReset{{356,484},{176,48}};
inline constexpr AudioSettingsRect kAudioSettingsBack{{780,484},{144,48}};
inline constexpr float kAudioSliderX = 544, kAudioSliderWidth = 340;
inline constexpr std::array<float,2> kAudioSliderY{310,398};
inline constexpr std::array<AudioSettingsRect,2> kAudioSliderHits{{
    {{kAudioSliderX-16,kAudioSliderY[0]-24},{kAudioSliderWidth+32,48}},
    {{kAudioSliderX-16,kAudioSliderY[1]-24},{kAudioSliderWidth+32,48}}
}};
}
