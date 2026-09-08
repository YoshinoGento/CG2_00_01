#include "application/magnet/system/GoalCelebrationEffectSystem.h"

#include "3d/LineDrawer.h"
#include "effect/ParticleManager.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr float kDurationSeconds = 0.72f;
constexpr float kTwoPi = 6.28318530717958647692f;
constexpr int kRingSegments = 32;
constexpr int kRayCount = 12;
float Saturate(float value) noexcept { return std::clamp(value, 0.0f, 1.0f); }
} // namespace

namespace magnet {
bool GoalCelebrationEffectSystem::Initialize(ParticleManager* particleManager) noexcept {
	Finalize();
	particleManager_ = particleManager;
	Reset();
	return particleManager_ != nullptr;
}

void GoalCelebrationEffectSystem::Finalize() noexcept {
	Reset();
	particleManager_ = nullptr;
}

void GoalCelebrationEffectSystem::Reset() noexcept {
	celebrations_.fill({});
	nextSlot_ = 0;
	particleSeed_ = 1;
}

void GoalCelebrationEffectSystem::Play(const Vector3& position, std::size_t scoredBallCount) {
	Celebration& effect = celebrations_[nextSlot_ % celebrations_.size()];
	effect.position = position + Vector3{ 0.0f, 0.32f, 0.0f };
	effect.elapsed = 0.0f;
	const std::size_t bonusCount = scoredBallCount > 0 ? scoredBallCount - 1 : 0;
	effect.intensity = 1.0f + 0.12f * static_cast<float>(
		(std::min)(bonusCount, std::size_t{ 3 }));
	effect.active = true;
	nextSlot_ = (nextSlot_ + 1) % celebrations_.size();

	if (!particleManager_) { return; }
	GPUParticleEmitSettings settings{};
	settings.translate = effect.position;
	settings.radius = 0.32f;
	settings.color = { 1.0f, 0.78f, 0.14f, 1.0f };
	settings.scale = { 0.13f, 0.13f, 0.13f };
	settings.lifeTime = 0.68f;
	settings.speed = 2.8f;
	settings.count = 52;
	settings.emit = 1;
	settings.extendedSettings = 1;
	settings.direction = { 0.0f, 1.0f, 0.0f };
	settings.directionSpread = 1.35f;
	settings.acceleration = { 0.0f, -0.65f, 0.0f };
	settings.drag = 0.7f;
	settings.endScale = { 0.02f, 0.02f, 0.02f };
	settings.endAlpha = 0.0f;
	settings.colorVariance = { 0.12f, 0.2f, 0.34f, 0.0f };
	settings.lifeTimeVariance = 0.16f;
	settings.speedVariance = 0.8f;
	settings.scaleVariance = 0.05f;
	settings.randomSeed = particleSeed_++;
	settings.fadeMode = 1;
	particleManager_->RequestGPUParticleEmit(settings);
}

void GoalCelebrationEffectSystem::Update(float deltaTime) noexcept {
	if (!std::isfinite(deltaTime) || deltaTime <= 0.0f) { return; }
	deltaTime = std::clamp(deltaTime, 0.0f, 0.1f);
	for (Celebration& effect : celebrations_) {
		if (!effect.active) { continue; }
		effect.elapsed += deltaTime;
		if (effect.elapsed >= kDurationSeconds) { effect.active = false; }
	}
}

void GoalCelebrationEffectSystem::Draw(LineDrawer& lineDrawer) const {
	for (const Celebration& effect : celebrations_) {
		if (!effect.active) { continue; }
		const float progress = Saturate(effect.elapsed / kDurationSeconds);
		const float remaining = 1.0f - progress;
		const float eased = 1.0f - (1.0f - progress) * (1.0f - progress);
		const float outerRadius = (0.18f + eased * 2.1f) * effect.intensity;
		const float innerRadius = (0.1f + eased * 1.35f) * effect.intensity;
		const auto drawRing = [&](float radius, float yOffset, Vector4 color) {
			color.w *= remaining;
			for (int segment = 0; segment < kRingSegments; ++segment) {
				const float a = kTwoPi * static_cast<float>(segment) / kRingSegments;
				const float b = kTwoPi * static_cast<float>(segment + 1) / kRingSegments;
				lineDrawer.DrawLine(
					effect.position + Vector3{ std::cos(a) * radius, yOffset, std::sin(a) * radius },
					effect.position + Vector3{ std::cos(b) * radius, yOffset, std::sin(b) * radius }, color);
			}
		};
		drawRing(outerRadius, 0.04f, { 1.0f, 0.72f, 0.08f, 1.0f });
		drawRing(innerRadius, 0.09f, { 0.2f, 0.95f, 1.0f, 0.9f });
		const float rayStart = innerRadius * 0.55f;
		const float rayEnd = outerRadius + remaining * 0.8f;
		for (int ray = 0; ray < kRayCount; ++ray) {
			const float angle = kTwoPi * static_cast<float>(ray) / kRayCount + 0.18f;
			const Vector3 direction{ std::cos(angle), 0.16f, std::sin(angle) };
			lineDrawer.DrawLine(effect.position + direction * rayStart,
				effect.position + direction * rayEnd, { 1.0f, 0.92f, 0.4f, remaining });
		}
	}
}
} // namespace magnet
