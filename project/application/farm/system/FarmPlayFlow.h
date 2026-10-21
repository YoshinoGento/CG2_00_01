#pragma once

// Transient session navigation only. Money/clear/save state belongs to existing Systems.
class FarmPlayFlow final {
public:
    enum class Phase { Briefing, Starting, Playing, Result, Review };
    void Reset() noexcept { phase_ = Phase::Briefing; returnTitle_ = false; }
    void RequestTitle() noexcept { returnTitle_ = true; }
    [[nodiscard]] bool RequestsTitle() const noexcept { return returnTitle_; }
    void Observe(bool cleared) noexcept {
        if (cleared && (phase_ == Phase::Briefing || phase_ == Phase::Playing)) phase_ = Phase::Result;
        else if (!cleared && (phase_ == Phase::Result || phase_ == Phase::Review)) phase_ = Phase::Briefing;
    }
    void Continue() noexcept {
        if (phase_ == Phase::Briefing) phase_ = Phase::Playing;
        else if (phase_ == Phase::Result) phase_ = Phase::Review;
    }
    void BeginStart() noexcept { if (phase_ == Phase::Briefing) phase_ = Phase::Starting; }
    void FinishStart() noexcept { if (phase_ == Phase::Starting) phase_ = Phase::Playing; }
    [[nodiscard]] Phase GetPhase() const noexcept { return phase_; }
    [[nodiscard]] bool BlocksSimulation() const noexcept {
        return returnTitle_ || phase_ == Phase::Briefing || phase_ == Phase::Starting || phase_ == Phase::Result;
    }
private:
    Phase phase_ = Phase::Briefing;
    bool returnTitle_ = false;
};
