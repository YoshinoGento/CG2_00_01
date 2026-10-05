#pragma once

#include "farm/core/FarmGrid.h"
#include "title/TitlePresentationLayout.h"
#include "math/Struct.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace title {

inline constexpr float kCycleSeconds = 40.0f;
inline constexpr float kFadeSeconds = 0.8f;
inline constexpr std::size_t kStarCount = 36;
inline constexpr std::size_t kTitleTileCount = 20;
inline constexpr float kCropCycleSeconds = 10.0f;

struct CropFrame {
    float growth = 0.0f;
    bool visible = false;
};

struct Star {
    Vector2 position{};
    float alpha = 0.0f;
    float size = 2.0f;
};

struct CelestialBodyFrame {
    Vector2 center{0.08f, 0.58f};
};

struct Frame {
    std::size_t skyFrom = 0;
    std::size_t skyTo = 1;
    float skyBlend = 0.0f;
    float skyOffset = 0.0f;
    float skyYaw = 0.0f;
    float cloudLift = 0.0f;
    float celestialAngle = 0.0f;
    float nightAmount = 0.0f;
    CelestialBodyFrame sun{}, moon{};
    Vector4 skyTop{0.12f, 0.42f, 0.65f, 1.0f};
    Vector4 skyHorizon{0.95f, 0.63f, 0.38f, 1.0f};
    Vector4 cloudColor{0.95f, 0.84f, 0.71f, 1.0f};
    Vector4 lightColor{1.0f, 1.0f, 1.0f, 1.0f};
    float groundExposure = .25f;
    Vector4 waterColor{0.2f, 0.6f, 0.8f, 1.0f};
    float waterPhase = 0.0f;
    float windAngle = 0.0f;
    float promptAlpha = 1.0f;
    float fadeAlpha = 1.0f;
    float logoOffsetY = 0.0f;
    float logoScale = 1.0f;
    std::array<CropFrame, kTitleTileCount> crops{};
    std::array<Star, kStarCount> stars{};
};

// Staged title data has no document/economy access and never advances a saved farm.
class TitlePresentationSystem final {
public:
    bool Initialize(std::uint32_t windSeed = 0x5A17u);
    void Update(float realDeltaSeconds) noexcept;
    void RequestStart() noexcept;
    bool ConsumeStart() noexcept;
    [[nodiscard]] Frame GetFrame() const noexcept;
    [[nodiscard]] const farm::FarmGrid& GetFarm() const noexcept { return farm_; }
    [[nodiscard]] bool IsLeaving() const noexcept { return leaving_; }

private:
    farm::FarmGrid farm_;
    float cycleTime_ = 0.0f;
    float entryTime_ = 0.0f;
    float leavingTime_ = 0.0f;
    bool leaving_ = false;
    bool startConsumed_ = false;
    Vector2 cloudOffset_{};
    Vector2 windVelocity_{};
    Vector2 windTarget_{};
    float windRemaining_ = 0.0f;
    std::uint32_t windSeed_ = 0x5A17u;
    float NextWindValue() noexcept;
};

} // namespace title
