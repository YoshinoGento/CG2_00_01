#include "application/magnet/system/MagneticImpactFeedbackSystem.h"

#include "3d/LineDrawer.h"

#include <algorithm>
#include <cmath>

namespace magnet {
namespace {

constexpr float kEffectDurationSeconds = 0.32f;
constexpr float kMinimumImpactSpeed = 2.0f;
constexpr float kFullIntensityImpactSpeed = 18.0f;
constexpr float kMaximumShakeDistance = 0.22f;
constexpr float kShakeDecayPerSecond = 5.0f;
constexpr float kTwoPi = 6.28318530717958647692f;
constexpr int kRingSegments = 20;
constexpr int kSparkCount = 8;

float Saturate(float value) noexcept
{
	return std::clamp(value, 0.0f, 1.0f);
}

} // namespace

void MagneticImpactFeedbackSystem::Reset() noexcept
{
	effects_.fill({});
	elapsedSeconds_ = 0.0f;
	shakeIntensity_ = 0.0f;
}

void MagneticImpactFeedbackSystem::AddImpacts(
	const MagneticImpactAttachmentSystem::ImpactEvents& events,
	std::size_t eventCount) noexcept
{
	eventCount = (std::min)(eventCount, events.size());
	for (std::size_t eventIndex = 0; eventIndex < eventCount; ++eventIndex) {
		const auto& event = events[eventIndex];
		if (!std::isfinite(event.position.x) || !std::isfinite(event.position.y) ||
			!std::isfinite(event.position.z) || !std::isfinite(event.relativeSpeed)) {
			continue;
		}
		Effect* target = nullptr;
		for (Effect& effect : effects_) {
			if (!effect.active) {
				target = &effect;
				break;
			}
		}
		if (!target) {
			target = &*std::max_element(effects_.begin(), effects_.end(),
				[](const Effect& left, const Effect& right) { return left.age < right.age; });
		}
		target->position = event.position;
		target->intensity = Saturate(
			(event.relativeSpeed - kMinimumImpactSpeed) /
			(kFullIntensityImpactSpeed - kMinimumImpactSpeed));
		target->age = 0.0f;
		target->active = true;
		shakeIntensity_ = (std::max)(shakeIntensity_, target->intensity);
	}
}

void MagneticImpactFeedbackSystem::Update(float deltaTime) noexcept
{
	if (!std::isfinite(deltaTime) || deltaTime <= 0.0f) {
		return;
	}
	elapsedSeconds_ += deltaTime;
	shakeIntensity_ = (std::max)(0.0f, shakeIntensity_ - kShakeDecayPerSecond * deltaTime);
	for (Effect& effect : effects_) {
		if (!effect.active) {
			continue;
		}
		effect.age += deltaTime;
		if (effect.age >= kEffectDurationSeconds) {
			effect.active = false;
		}
	}
}

void MagneticImpactFeedbackSystem::Draw(LineDrawer& lineDrawer) const
{
	for (const Effect& effect : effects_) {
		if (!effect.active) {
			continue;
		}
		const float progress = Saturate(effect.age / kEffectDurationSeconds);
		const float remaining = 1.0f - progress;
		const float radius = 0.35f + progress * (0.8f + effect.intensity * 0.7f);
		const Vector4 color = { 1.0f, 0.78f, 0.18f, remaining };
		for (int segment = 0; segment < kRingSegments; ++segment) {
			const float firstAngle = kTwoPi * static_cast<float>(segment) / kRingSegments;
			const float secondAngle = kTwoPi * static_cast<float>(segment + 1) / kRingSegments;
			lineDrawer.DrawLine(
				effect.position + Vector3{ std::cos(firstAngle) * radius, 0.03f, std::sin(firstAngle) * radius },
				effect.position + Vector3{ std::cos(secondAngle) * radius, 0.03f, std::sin(secondAngle) * radius },
				color);
		}
		const float sparkLength = remaining * (0.35f + effect.intensity * 0.65f);
		for (int spark = 0; spark < kSparkCount; ++spark) {
			const float angle = kTwoPi * static_cast<float>(spark) / kSparkCount + effect.intensity * 0.31f;
			const Vector3 direction = { std::cos(angle), 0.18f, std::sin(angle) };
			lineDrawer.DrawLine(
				effect.position + direction * (radius * 0.35f),
				effect.position + direction * (radius * 0.35f + sparkLength),
				{ 1.0f, 0.35f, 0.08f, remaining });
		}
	}
}

Vector3 MagneticImpactFeedbackSystem::GetCameraShakeOffset() const noexcept
{
	if (shakeIntensity_ <= 0.0f) {
		return {};
	}
	const float amplitude = kMaximumShakeDistance * shakeIntensity_;
	return {
		std::sin(elapsedSeconds_ * 91.0f) * amplitude,
		std::sin(elapsedSeconds_ * 117.0f + 1.3f) * amplitude * 0.35f,
		std::cos(elapsedSeconds_ * 103.0f) * amplitude,
	};
}

std::size_t MagneticImpactFeedbackSystem::GetActiveEffectCount() const noexcept
{
	return static_cast<std::size_t>(std::count_if(
		effects_.begin(), effects_.end(), [](const Effect& effect) { return effect.active; }));
}

} // namespace magnet
