#include "base/FrameClock.h"

#include <algorithm>
#include <cmath>

void FrameClock::Initialize() {
	previousTime_ = Clock::now();
	accumulatorSeconds_ = 0.0;
	droppedSimulationSeconds_ = 0.0;
	realDeltaSeconds_ = kDefaultFixedDeltaSeconds;
	frameDeltaSeconds_ = kDefaultFixedDeltaSeconds;
	fixedDeltaSeconds_ = kDefaultFixedDeltaSeconds;
	timeScale_ = 1.0f;
	simulationTick_ = 0;
	fixedStepsThisFrame_ = 0;
	initialized_ = true;
	paused_ = false;
	singleStepRequested_ = false;
}

void FrameClock::Tick() {
	if (!initialized_) {
		Initialize();
		return;
	}

	const Clock::time_point now = Clock::now();
	double elapsedSeconds = std::chrono::duration<double>(now - previousTime_).count();
	previousTime_ = now;
	if (!std::isfinite(elapsedSeconds) || elapsedSeconds < 0.0) {
		elapsedSeconds = 0.0;
	}
	elapsedSeconds = std::clamp(
		elapsedSeconds, 0.0, static_cast<double>(kMaximumFrameDeltaSeconds));
	realDeltaSeconds_ = static_cast<float>(elapsedSeconds);
	// A paused Step advances simulation time once; real time keeps tracking the editor.
	const bool advanceSingleStep = paused_ && singleStepRequested_;
	frameDeltaSeconds_ = paused_
		? (advanceSingleStep ? fixedDeltaSeconds_ : 0.0f)
		: realDeltaSeconds_ * timeScale_;
	fixedStepsThisFrame_ = 0;

	if (paused_) {
		if (advanceSingleStep) {
			accumulatorSeconds_ += fixedDeltaSeconds_;
		}
	} else {
		accumulatorSeconds_ += frameDeltaSeconds_;
	}
	singleStepRequested_ = false;
}

bool FrameClock::ConsumeFixedStep() {
	const double fixedDelta = static_cast<double>(fixedDeltaSeconds_);
	if (accumulatorSeconds_ + 1.0e-9 < fixedDelta) {
		return false;
	}
	if (fixedStepsThisFrame_ >= kMaximumFixedStepsPerFrame) {
		const double retained = std::fmod(accumulatorSeconds_, fixedDelta);
		droppedSimulationSeconds_ += accumulatorSeconds_ - retained;
		accumulatorSeconds_ = retained;
		return false;
	}

	accumulatorSeconds_ -= fixedDelta;
	++fixedStepsThisFrame_;
	++simulationTick_;
	return true;
}

void FrameClock::SetTimeScale(float timeScale) noexcept {
	if (!std::isfinite(timeScale)) {
		return;
	}
	timeScale_ = std::clamp(timeScale, 0.0f, 4.0f);
}

float FrameClock::GetInterpolationAlpha() const noexcept {
	if (fixedDeltaSeconds_ <= 0.0f) {
		return 0.0f;
	}
	return std::clamp(
		static_cast<float>(accumulatorSeconds_ / fixedDeltaSeconds_), 0.0f, 1.0f);
}
