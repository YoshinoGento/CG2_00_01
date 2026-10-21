#pragma once
#include "base/InitializationTask.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>

namespace scene {
class SceneLoadSystem final {
public:
    enum class Status { Idle, Loading, Ready, Failed };
    struct SlowStep { std::size_t index = 0; double milliseconds = 0; };
    struct Metrics {
        std::size_t frames = 0, steps = 0;
        double workMs = 0, longestStepMs = 0;
        std::array<SlowStep, 8> slowest{};
    };
    static constexpr double kFrameBudgetMs = 4.0;
    static constexpr std::size_t kMaximumStepsPerFrame = 64;

    bool Begin(InitializationTask task) {
        if (status_ == Status::Loading) return false;
        task_ = std::move(task); metrics_ = {}; status_ = Status::Loading;
        return true;
    }
    void Advance() noexcept {
        if (status_ != Status::Loading) return;
        ++metrics_.frames;
        const auto frameStart = Clock::now();
        try {
            for (std::size_t step = 0; step < kMaximumStepsPerFrame && !task_.Done(); ++step) {
                const auto start = Clock::now();
                task_.ResumeOne();
                const double elapsed = Milliseconds(Clock::now() - start);
                ++metrics_.steps;
                metrics_.workMs += elapsed;
                metrics_.longestStepMs = (std::max)(metrics_.longestStepMs, elapsed);
                SlowStep sample{metrics_.steps, elapsed};
                for (auto& saved : metrics_.slowest) {
                    if (sample.milliseconds > saved.milliseconds) std::swap(sample, saved);
                }
                // A file decode or driver call is indivisible: this is a soft budget.
                if (Milliseconds(Clock::now() - frameStart) >= kFrameBudgetMs) break;
            }
            if (task_.Done()) {
                status_ = task_.Result() ? Status::Ready : Status::Failed;
                task_.Reset();
            }
        } catch (...) { task_.Reset(); status_ = Status::Failed; }
    }
    void Cancel() noexcept { task_.Reset(); status_ = Status::Idle; }
    [[nodiscard]] bool BlocksScene() const noexcept { return status_ == Status::Loading || status_ == Status::Failed; }
    [[nodiscard]] Status GetStatus() const noexcept { return status_; }
    [[nodiscard]] const Metrics& GetMetrics() const noexcept { return metrics_; }
private:
    using Clock = std::chrono::steady_clock;
    static double Milliseconds(Clock::duration duration) noexcept {
        return std::chrono::duration<double, std::milli>(duration).count();
    }
    InitializationTask task_;
    Status status_ = Status::Idle;
    Metrics metrics_{};
};
}
