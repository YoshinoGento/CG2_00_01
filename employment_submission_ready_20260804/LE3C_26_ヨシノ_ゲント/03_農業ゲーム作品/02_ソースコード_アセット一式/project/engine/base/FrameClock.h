#pragma once

#include <chrono>
#include <cstdint>

class FrameClock final {
public:
	static constexpr float kDefaultFixedDeltaSeconds = 1.0f / 60.0f;
	static constexpr float kMaximumFrameDeltaSeconds = 0.25f;
	static constexpr uint32_t kMaximumFixedStepsPerFrame = 8;

	void Initialize();
	void Tick();
	[[nodiscard]] bool ConsumeFixedStep();

	void SetPaused(bool paused) noexcept { paused_ = paused; }
	void TogglePaused() noexcept { paused_ = !paused_; }
	void RequestSingleStep() noexcept { singleStepRequested_ = true; }
	void SetTimeScale(float timeScale) noexcept;

	[[nodiscard]] bool IsPaused() const noexcept { return paused_; }
	[[nodiscard]] float GetTimeScale() const noexcept { return timeScale_; }
	[[nodiscard]] float GetRealDeltaSeconds() const noexcept { return realDeltaSeconds_; }
	[[nodiscard]] float GetFrameDeltaSeconds() const noexcept { return frameDeltaSeconds_; }
	[[nodiscard]] float GetFixedDeltaSeconds() const noexcept { return fixedDeltaSeconds_; }
	[[nodiscard]] float GetInterpolationAlpha() const noexcept;
	[[nodiscard]] uint64_t GetSimulationTick() const noexcept { return simulationTick_; }
	[[nodiscard]] uint32_t GetFixedStepsThisFrame() const noexcept { return fixedStepsThisFrame_; }
	[[nodiscard]] double GetDroppedSimulationSeconds() const noexcept { return droppedSimulationSeconds_; }

private:
	using Clock = std::chrono::steady_clock;
	Clock::time_point previousTime_{};
	double accumulatorSeconds_ = 0.0;
	double droppedSimulationSeconds_ = 0.0;
	float realDeltaSeconds_ = kDefaultFixedDeltaSeconds;
	float frameDeltaSeconds_ = kDefaultFixedDeltaSeconds;
	float fixedDeltaSeconds_ = kDefaultFixedDeltaSeconds;
	float timeScale_ = 1.0f;
	uint64_t simulationTick_ = 0;
	uint32_t fixedStepsThisFrame_ = 0;
	bool initialized_ = false;
	bool paused_ = false;
	bool singleStepRequested_ = false;
};
