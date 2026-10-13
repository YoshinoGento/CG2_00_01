#pragma once
#include <algorithm>
#include <cmath>
#include <array>

namespace scene {
inline constexpr float kDropFallSeconds = .18f;
inline constexpr float kRipplePreviewSeconds = .55f;
inline constexpr float kRippleWipeSeconds = .35f;
inline constexpr float kRippleCoverSeconds = kDropFallSeconds+kRipplePreviewSeconds+kRippleWipeSeconds;
inline constexpr float kRippleRevealSeconds = .35f;
inline constexpr float kDropLifetimeSeconds = 2.4f;
inline constexpr std::size_t kMaximumWaterDrops = 4;
inline constexpr float kTransitionMaximumDelta = .25f;
inline constexpr float kRippleCenterX = .46f;
inline constexpr float kRippleCenterY = .67f;

enum class TransitionPhase { Idle, Covering, Revealing, WaitingForRelease };
struct RipplePoint { float x = kRippleCenterX, y = kRippleCenterY; };
struct WaterDrop { RipplePoint center{}; float age = 0; bool active = false; };
struct TransitionFrame {
    TransitionPhase phase = TransitionPhase::Idle;
    float progress = 0;
    RipplePoint center{};
    std::array<WaterDrop,kMaximumWaterDrops> drops{};
};

// No scene/resource ownership; presentation acknowledgement gates the swap, not elapsed time alone.
class SceneTransitionSystem final {
public:
    bool Begin(RipplePoint point = {}) noexcept {
        if (IsActive()) return false;
        ClearDrops();
        center_ = Sanitize(point);
        drops_[0] = {center_,0,true};
        phase_ = TransitionPhase::Covering;
        time_ = 0;
        coveredPresented_ = false;
        return true;
    }
    void Update(float delta, bool startInputsReleased) noexcept {
        if (!std::isfinite(delta) || delta < 0) return;
        delta = (std::min)(delta, kTransitionMaximumDelta);
        for (auto& drop : drops_) if (drop.active) {
            drop.age = (std::min)(drop.age+delta,kDropLifetimeSeconds);
            if (drop.age >= kDropLifetimeSeconds) drop.active = false;
        }
        if (phase_ == TransitionPhase::Covering) time_ = (std::min)(time_+delta,kRippleCoverSeconds);
        else if (phase_ == TransitionPhase::Revealing) {
            time_ = (std::min)(time_+delta,kRippleRevealSeconds);
            if (time_ >= kRippleRevealSeconds) phase_ = TransitionPhase::WaitingForRelease;
        } else if (phase_ == TransitionPhase::WaitingForRelease && startInputsReleased) phase_ = TransitionPhase::Idle;
    }
    bool AddDrop(RipplePoint point) noexcept {
        if (IsActive() || !std::isfinite(point.x) || !std::isfinite(point.y) ||
            point.x < 0 || point.x > 1 || point.y < 0 || point.y > 1) return false;
        for (auto& drop : drops_) if (!drop.active) { drop = {point,0,true}; return true; }
        return false; // Keep existing waves intact under rapid repeated clicks.
    }
    void ClearDrops() noexcept { drops_ = {}; }
    void NotifyCoverPresented() noexcept {
        if (phase_ == TransitionPhase::Covering && time_ >= kRippleCoverSeconds) coveredPresented_ = true;
    }
    bool BeginReveal() noexcept {
        if (!CanSwap()) return false;
        phase_ = TransitionPhase::Revealing;
        time_ = 0;
        coveredPresented_ = false;
        ClearDrops();
        return true;
    }
    [[nodiscard]] bool IsActive() const noexcept { return phase_ != TransitionPhase::Idle; }
    [[nodiscard]] bool IsCovering() const noexcept { return phase_ == TransitionPhase::Covering; }
    [[nodiscard]] bool CanSwap() const noexcept { return IsCovering() && coveredPresented_; }
    [[nodiscard]] bool BlocksIncomingUpdate() const noexcept { return IsActive() && !IsCovering(); }
    [[nodiscard]] TransitionFrame GetFrame() const noexcept {
        float progress = 0;
        if (IsCovering()) progress = time_ >= kRippleCoverSeconds ? 1.0f :
            (time_-kDropFallSeconds-kRipplePreviewSeconds)/kRippleWipeSeconds;
        else if (phase_ == TransitionPhase::Revealing) progress = time_/kRippleRevealSeconds;
        else if (phase_ == TransitionPhase::WaitingForRelease) progress = 1;
        progress = std::clamp(progress,0.0f,1.0f);
        return {phase_,progress*progress*(3-2*progress),center_,drops_};
    }
private:
    static RipplePoint Sanitize(RipplePoint point) noexcept {
        if (!std::isfinite(point.x) || !std::isfinite(point.y)) return {};
        return {std::clamp(point.x,0.0f,1.0f),std::clamp(point.y,0.0f,1.0f)};
    }
    TransitionPhase phase_ = TransitionPhase::Idle;
    float time_ = 0;
    bool coveredPresented_ = false;
    RipplePoint center_{};
    std::array<WaterDrop,kMaximumWaterDrops> drops_{};
};
}
