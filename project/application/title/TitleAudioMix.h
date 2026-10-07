#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace title {

inline constexpr float kCycleSeconds = 40.0f;
inline constexpr std::size_t kAmbienceCount = 4;
inline constexpr float kAudioCrossfadeSeconds = 2.5f;
inline constexpr float kAmbienceGain = .45f;
inline constexpr float kWaterGain = .30f;
static_assert(kAudioCrossfadeSeconds < kCycleSeconds / kAmbienceCount);

struct AudioMix {
    std::array<float, kAmbienceCount> ambience{};
    float water = 0;
};

[[nodiscard]] inline AudioMix SanitizeAudioMix(AudioMix mix) noexcept {
    float total = 0;
    for (auto& gain : mix.ambience) {
        gain = std::isfinite(gain) ? std::clamp(gain, 0.0f, kAmbienceGain) : 0;
        total += gain;
    }
    if (total > kAmbienceGain) {
        for (auto& gain : mix.ambience) gain *= kAmbienceGain / total;
    }
    mix.water = std::isfinite(mix.water) ? std::clamp(mix.water, 0.0f, kWaterGain) : 0;
    return mix;
}

// Same clock as the sky. Unity-sum weights avoid increasing gain during overlap.
[[nodiscard]] inline AudioMix MakeAudioMix(float cycleTime, float fadeAlpha) noexcept {
    AudioMix mix;
    if (!std::isfinite(cycleTime) || !std::isfinite(fadeAlpha) || cycleTime < 0) return mix;
    constexpr float segment = kCycleSeconds / kAmbienceCount;
    const float time = std::fmod(cycleTime, kCycleSeconds);
    const auto current = static_cast<std::size_t>(time / segment) % kAmbienceCount;
    const auto next = (current + 1) % kAmbienceCount;
    const float t = std::clamp((std::fmod(time, segment) - (segment-kAudioCrossfadeSeconds)) /
        kAudioCrossfadeSeconds, 0.0f, 1.0f);
    const float blend = t*t*(3-2*t);
    const float visibility = 1-std::clamp(fadeAlpha, 0.0f, 1.0f);
    mix.ambience[current] = kAmbienceGain * visibility * (1-blend);
    mix.ambience[next] = kAmbienceGain * visibility * blend;
    mix.water = kWaterGain * visibility;
    return mix;
}

} // namespace title
