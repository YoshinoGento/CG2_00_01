#pragma once
#include <algorithm>
#include <cmath>

namespace scene {
inline constexpr float kRippleCoverSeconds = .35f;
inline constexpr float kRippleRevealSeconds = .35f;
inline constexpr float kTransitionMaximumDelta = .25f;
inline constexpr float kRippleCenterX = .46f;
inline constexpr float kRippleCenterY = .67f;

enum class TransitionPhase { Idle, Covering, Revealing, WaitingForRelease };
struct TransitionFrame {
    TransitionPhase phase = TransitionPhase::Idle;
    float progress = 0;
};

// No scene/resource ownership; presentation acknowledgement gates the swap, not elapsed time alone.
class SceneTransitionSystem final {
public:
    bool Begin() noexcept {
        if (IsActive()) return false;
        phase_ = TransitionPhase::Covering;
        time_ = 0;
        coveredPresented_ = false;
        return true;
    }
    void Update(float delta, bool startInputsReleased) noexcept {
        if (!std::isfinite(delta) || delta < 0) return;
        delta = (std::min)(delta, kTransitionMaximumDelta);
        if (phase_ == TransitionPhase::Covering) time_ = (std::min)(time_+delta,kRippleCoverSeconds);
        else if (phase_ == TransitionPhase::Revealing) {
            time_ = (std::min)(time_+delta,kRippleRevealSeconds);
            if (time_ >= kRippleRevealSeconds) phase_ = TransitionPhase::WaitingForRelease;
        } else if (phase_ == TransitionPhase::WaitingForRelease && startInputsReleased) phase_ = TransitionPhase::Idle;
    }
    void NotifyCoverPresented() noexcept {
        if (phase_ == TransitionPhase::Covering && time_ >= kRippleCoverSeconds) coveredPresented_ = true;
    }
    bool BeginReveal() noexcept {
        if (!CanSwap()) return false;
        phase_ = TransitionPhase::Revealing;
        time_ = 0;
        coveredPresented_ = false;
        return true;
    }
    [[nodiscard]] bool IsActive() const noexcept { return phase_ != TransitionPhase::Idle; }
    [[nodiscard]] bool IsCovering() const noexcept { return phase_ == TransitionPhase::Covering; }
    [[nodiscard]] bool CanSwap() const noexcept { return IsCovering() && coveredPresented_; }
    [[nodiscard]] bool BlocksIncomingUpdate() const noexcept { return IsActive() && !IsCovering(); }
    [[nodiscard]] TransitionFrame GetFrame() const noexcept {
        float progress = 0;
        if (IsCovering()) progress = time_/kRippleCoverSeconds;
        else if (phase_ == TransitionPhase::Revealing) progress = time_/kRippleRevealSeconds;
        else if (phase_ == TransitionPhase::WaitingForRelease) progress = 1;
        progress = std::clamp(progress,0.0f,1.0f);
        return {phase_,progress*progress*(3-2*progress)};
    }
private:
    TransitionPhase phase_ = TransitionPhase::Idle;
    float time_ = 0;
    bool coveredPresented_ = false;
};
}
