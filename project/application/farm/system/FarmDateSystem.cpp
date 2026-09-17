#include "farm/system/FarmDateSystem.h"

#include "base/Logger.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
	constexpr int kInitialDay = 1;
	constexpr float kInitialElapsedSeconds = 0.0f;
	constexpr float kDayLengthSeconds = 60.0f;
	constexpr float kDefaultTimeScale = 1.0f;
	constexpr float kTimeScale2x = 2.0f;
	constexpr float kTimeScale4x = 4.0f;
	constexpr float kTimeScaleEpsilon = 0.001f;

	bool IsSameTimeScale(float lhs, float rhs)
	{
		return std::fabs(lhs - rhs) <= kTimeScaleEpsilon;
	}

	bool IsSupportedTimeScale(float timeScale)
	{
		return IsSameTimeScale(timeScale, kDefaultTimeScale) ||
			IsSameTimeScale(timeScale, kTimeScale2x) ||
			IsSameTimeScale(timeScale, kTimeScale4x);
	}
}

void FarmDateSystem::Initialize()
{
	day_ = kInitialDay;
	elapsedSecondsInDay_ = kInitialElapsedSeconds;
	dayLengthSeconds_ = kDayLengthSeconds;
	timeScale_ = kDefaultTimeScale;
}

void FarmDateSystem::Update(float deltaTime, int stopAtDay)
{
	if (!std::isfinite(deltaTime) || deltaTime <= 0.0f) {
		return;
	}

	const double elapsed = elapsedSecondsInDay_ + static_cast<double>(deltaTime) * timeScale_;
	// Drop overshoot when an event boundary is reached; resume starts on that day.
	if (stopAtDay > day_ && elapsed >= static_cast<double>(stopAtDay - day_) * dayLengthSeconds_) {
		day_ = stopAtDay;
		elapsedSecondsInDay_ = 0.0f;
		return;
	}
	const double days = std::floor(elapsed / dayLengthSeconds_);
	const int remainingDays = (std::numeric_limits<int>::max)() - day_;
	if (days >= remainingDays) {
		day_ = (std::numeric_limits<int>::max)(); elapsedSecondsInDay_ = 0.0f;
		return;
	}
	day_ += static_cast<int>(days);
	elapsedSecondsInDay_ = (std::min)(static_cast<float>(std::fmod(elapsed, dayLengthSeconds_)),
		std::nextafter(dayLengthSeconds_, 0.0f));
}

void FarmDateSystem::SetTimeScale(float timeScale)
{
	if (!IsSupportedTimeScale(timeScale)) {
		Logger::Log("FarmDateSystem::SetTimeScale ignored. Supported values are 1.0, 2.0, and 4.0.");
		return;
	}

	timeScale_ = timeScale;
}

void FarmDateSystem::CycleTimeScale()
{
	if (IsSameTimeScale(timeScale_, kDefaultTimeScale)) {
		timeScale_ = kTimeScale2x;
		return;
	}
	if (IsSameTimeScale(timeScale_, kTimeScale2x)) {
		timeScale_ = kTimeScale4x;
		return;
	}

	timeScale_ = kDefaultTimeScale;
}

void FarmDateSystem::AdvanceOneDay()
{
	if (day_ < (std::numeric_limits<int>::max)()) ++day_;
	elapsedSecondsInDay_ = 0.0f;
}

float FarmDateSystem::GetDayProgress() const
{
	if (dayLengthSeconds_ <= 0.0f) {
		return 0.0f;
	}

	return std::clamp(elapsedSecondsInDay_ / dayLengthSeconds_, 0.0f, 1.0f);
}

FarmDateSystem::Snapshot FarmDateSystem::CaptureSnapshot() const noexcept
{
	return { day_, elapsedSecondsInDay_, timeScale_ };
}

bool FarmDateSystem::RestoreSnapshot(const Snapshot& snapshot)
{
	if (snapshot.day < kInitialDay || !std::isfinite(snapshot.elapsedSecondsInDay) ||
		snapshot.elapsedSecondsInDay < 0.0f || snapshot.elapsedSecondsInDay >= dayLengthSeconds_ ||
		!IsSupportedTimeScale(snapshot.timeScale)) {
		return false;
	}
	day_ = snapshot.day;
	elapsedSecondsInDay_ = snapshot.elapsedSecondsInDay;
	timeScale_ = snapshot.timeScale;
	return true;
}
