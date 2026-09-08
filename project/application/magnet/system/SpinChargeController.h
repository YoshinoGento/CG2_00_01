#pragma once

#include "math/Struct.h"

namespace magnet {

// Accumulates absolute player rotation and turns it into a one-shot launch
// multiplier. This keeps the optional arcade behavior outside the core chain
// and momentum simulation.
class SpinChargeController final {
public:
	struct Settings {
		bool enabled = false;
		float rotationsForFullCharge = 3.0f;
		float maximumTurnSpeedMultiplier = 4.0f;
		float maximumSpeedMultiplier = 8.0f;
		float maximumLaunchSpeed = 96.0f;
		float minimumBallSpeedForBoost = 1.5f;
		float ballSpeedForFullBoost = 14.0f;
	};

	void SetSettings(const Settings& settings) noexcept;
	void Reset() noexcept;
	void ResetCharge() noexcept;
	[[nodiscard]] bool Update(float playerHeadingRadians, float deltaTime) noexcept;
	[[nodiscard]] Vector3 ApplyToLaunchVelocity(const Vector3& velocity) const noexcept;

	[[nodiscard]] const Settings& GetSettings() const noexcept { return settings_; }
	[[nodiscard]] float GetAccumulatedRotationRadians() const noexcept {
		return accumulatedRotationRadians_;
	}
	[[nodiscard]] float GetChargeRatio() const noexcept;
	[[nodiscard]] float GetTurnSpeedMultiplier() const noexcept;
	[[nodiscard]] float GetSpeedMultiplier() const noexcept;

private:
	Settings settings_{};
	float previousHeadingRadians_ = 0.0f;
	float accumulatedRotationRadians_ = 0.0f;
	bool hasPreviousHeading_ = false;
};

} // namespace magnet
