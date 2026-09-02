#include "application/magnet/system/SpinChargeController.h"

#include <algorithm>
#include <cmath>

namespace magnet {
namespace {

constexpr float kTwoPi = 6.28318530717958647692f;
constexpr float kMinimumFullChargeRotations = 0.25f;
constexpr float kMaximumFullChargeRotations = 12.0f;
constexpr float kMinimumSpeedMultiplier = 1.0f;
constexpr float kMaximumSpeedMultiplier = 16.0f;
constexpr float kMaximumTurnSpeedMultiplier = 12.0f;
constexpr float kMinimumLaunchSpeed = 22.0f;
constexpr float kMaximumLaunchSpeed = 160.0f;
constexpr float kMaximumBallSpeedThreshold = 40.0f;
constexpr float kDirectionEpsilonSquared = 1.0e-8f;

bool IsFinite(const Vector3& value) noexcept
{
	return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

} // namespace

void SpinChargeController::SetSettings(const Settings& settings) noexcept
{
	settings_.enabled = settings.enabled;
	settings_.rotationsForFullCharge = std::isfinite(settings.rotationsForFullCharge)
		? std::clamp(
			settings.rotationsForFullCharge,
			kMinimumFullChargeRotations,
			kMaximumFullChargeRotations)
		: 3.0f;
	settings_.maximumSpeedMultiplier = std::isfinite(settings.maximumSpeedMultiplier)
		? std::clamp(
			settings.maximumSpeedMultiplier,
			kMinimumSpeedMultiplier,
			kMaximumSpeedMultiplier)
		: 8.0f;
	settings_.maximumTurnSpeedMultiplier = std::isfinite(settings.maximumTurnSpeedMultiplier)
		? std::clamp(
			settings.maximumTurnSpeedMultiplier,
			kMinimumSpeedMultiplier,
			kMaximumTurnSpeedMultiplier)
		: 4.0f;
	settings_.maximumLaunchSpeed = std::isfinite(settings.maximumLaunchSpeed)
		? std::clamp(
			settings.maximumLaunchSpeed,
			kMinimumLaunchSpeed,
			kMaximumLaunchSpeed)
		: 96.0f;
	settings_.minimumBallSpeedForBoost = std::isfinite(settings.minimumBallSpeedForBoost)
		? std::clamp(
			settings.minimumBallSpeedForBoost,
			0.0f,
			kMaximumBallSpeedThreshold - 0.1f)
		: 1.5f;
	settings_.ballSpeedForFullBoost = std::isfinite(settings.ballSpeedForFullBoost)
		? std::clamp(
			settings.ballSpeedForFullBoost,
			settings_.minimumBallSpeedForBoost + 0.1f,
			kMaximumBallSpeedThreshold)
		: 14.0f;
	if (!settings_.enabled) {
		Reset();
	}
}

void SpinChargeController::Reset() noexcept
{
	previousHeadingRadians_ = 0.0f;
	accumulatedRotationRadians_ = 0.0f;
	hasPreviousHeading_ = false;
}

void SpinChargeController::ResetCharge() noexcept
{
	accumulatedRotationRadians_ = 0.0f;
}

bool SpinChargeController::Update(float playerHeadingRadians, float deltaTime) noexcept
{
	if (!std::isfinite(playerHeadingRadians) || !std::isfinite(deltaTime) ||
		deltaTime <= 0.0f) {
		return false;
	}
	if (!settings_.enabled) {
		Reset();
		return true;
	}
	if (!hasPreviousHeading_) {
		previousHeadingRadians_ = playerHeadingRadians;
		hasPreviousHeading_ = true;
		return true;
	}

	const float rotationDelta = std::remainder(
		playerHeadingRadians - previousHeadingRadians_,
		kTwoPi);
	previousHeadingRadians_ = playerHeadingRadians;
	if (!std::isfinite(rotationDelta)) {
		return false;
	}
	const float fullChargeRadians = settings_.rotationsForFullCharge * kTwoPi;
	accumulatedRotationRadians_ = (std::min)(
		accumulatedRotationRadians_ + std::abs(rotationDelta),
		fullChargeRadians);
	return std::isfinite(accumulatedRotationRadians_);
}

Vector3 SpinChargeController::ApplyToLaunchVelocity(const Vector3& velocity) const noexcept
{
	if (!IsFinite(velocity) || !settings_.enabled) {
		return velocity;
	}
	const float sourceSpeedSquared =
		velocity.x * velocity.x + velocity.z * velocity.z;
	if (!std::isfinite(sourceSpeedSquared) || sourceSpeedSquared <= kDirectionEpsilonSquared) {
		return {};
	}
	const float sourceSpeed = std::sqrt(sourceSpeedSquared);
	const float boostSpeedRange = (std::max)(
		settings_.ballSpeedForFullBoost - settings_.minimumBallSpeedForBoost,
		0.1f);
	const float ballActivity = std::clamp(
		(sourceSpeed - settings_.minimumBallSpeedForBoost) / boostSpeedRange,
		0.0f,
		1.0f);
	const float effectiveCharge = GetChargeRatio() * ballActivity;
	const float effectiveChargeEased = effectiveCharge * effectiveCharge;
	const float individualMultiplier = 1.0f +
		(settings_.maximumSpeedMultiplier - 1.0f) * effectiveChargeEased;
	Vector3 boostedVelocity = velocity * individualMultiplier;
	boostedVelocity.y = velocity.y;
	const float speedSquared =
		boostedVelocity.x * boostedVelocity.x + boostedVelocity.z * boostedVelocity.z;
	const float maximumSpeedSquared =
		settings_.maximumLaunchSpeed * settings_.maximumLaunchSpeed;
	if (speedSquared > maximumSpeedSquared) {
		const float scale = settings_.maximumLaunchSpeed / std::sqrt(speedSquared);
		boostedVelocity.x *= scale;
		boostedVelocity.z *= scale;
	}
	return boostedVelocity;
}

float SpinChargeController::GetChargeRatio() const noexcept
{
	if (!settings_.enabled) {
		return 0.0f;
	}
	const float fullChargeRadians = settings_.rotationsForFullCharge * kTwoPi;
	return fullChargeRadians > 0.0f
		? std::clamp(accumulatedRotationRadians_ / fullChargeRadians, 0.0f, 1.0f)
		: 0.0f;
}

float SpinChargeController::GetSpeedMultiplier() const noexcept
{
	const float charge = GetChargeRatio();
	const float easedCharge = charge * charge;
	return 1.0f +
		(settings_.maximumSpeedMultiplier - 1.0f) * easedCharge;
}

float SpinChargeController::GetTurnSpeedMultiplier() const noexcept
{
	const float charge = GetChargeRatio();
	return 1.0f +
		(settings_.maximumTurnSpeedMultiplier - 1.0f) * charge;
}

} // namespace magnet
