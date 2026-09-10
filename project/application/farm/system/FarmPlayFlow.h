#pragma once

// Transient session navigation only. Money/clear/save state belongs to existing Systems.
class FarmPlayFlow final {
public:
    enum class Phase { Briefing, Playing, Result, Review };
    void Reset() noexcept { phase_ = Phase::Briefing; }
    void Observe(bool cleared) noexcept {
        if (cleared && (phase_ == Phase::Briefing || phase_ == Phase::Playing)) phase_ = Phase::Result;
        else if (!cleared && (phase_ == Phase::Result || phase_ == Phase::Review)) phase_ = Phase::Briefing;
    }
    void Continue() noexcept {
        if (phase_ == Phase::Briefing) phase_ = Phase::Playing;
        else if (phase_ == Phase::Result) phase_ = Phase::Review;
    }
    [[nodiscard]] Phase GetPhase() const noexcept { return phase_; }
    [[nodiscard]] bool BlocksSimulation() const noexcept {
        return phase_ == Phase::Briefing || phase_ == Phase::Result;
    }
private:
    Phase phase_ = Phase::Briefing;
};
