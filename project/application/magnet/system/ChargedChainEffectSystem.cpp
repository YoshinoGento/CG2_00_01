#include "application/magnet/system/ChargedChainEffectSystem.h"

#include "3d/LineDrawer.h"
#include "application/magnet/system/MagnetChainSystem.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace {
constexpr float kTwoPi = 6.28318530717958647692f;
constexpr int kArcSegments = 4;
constexpr int kHaloSegments = 12;

uint32_t Hash(uint32_t value) noexcept {
	value ^= value >> 16;
	value *= 0x7feb352du;
	value ^= value >> 15;
	value *= 0x846ca68bu;
	return value ^ (value >> 16);
}

float SignedNoise(uint32_t seed) noexcept {
	return static_cast<float>(Hash(seed) & 0xffffu) / 32767.5f - 1.0f;
}

Vector3 NormalizeOr(const Vector3& value, const Vector3& fallback) noexcept {
	const float lengthSquared = value.x * value.x + value.y * value.y + value.z * value.z;
	if (!std::isfinite(lengthSquared) || lengthSquared <= 0.000001f) { return fallback; }
	return value / std::sqrt(lengthSquared);
}
} // namespace

namespace magnet {
void ChargedChainEffectSystem::Reset() noexcept {
	elapsedSeconds_ = 0.0f;
}

void ChargedChainEffectSystem::Update(float deltaTime) noexcept {
	if (!std::isfinite(deltaTime) || deltaTime <= 0.0f) { return; }
	elapsedSeconds_ += std::clamp(deltaTime, 0.0f, 0.1f);
}

void ChargedChainEffectSystem::Draw(
	const MagnetChainSystem& chain, LineDrawer& lineDrawer) const {
	const std::size_t attachedCount = chain.GetAttachedBallCount();
	if (attachedCount == 0) { return; }
	const float charge = std::clamp(
		static_cast<float>(attachedCount) /
		static_cast<float>(MagnetChainSystem::kLinksPerSide * 2), 0.0f, 1.0f);
	const uint32_t phase = static_cast<uint32_t>(elapsedSeconds_ * (22.0f + charge * 24.0f));
	const auto* player = chain.GetPhysicsWorld().GetBody(chain.GetPlayerBody());
	if (!player || !player->active) { return; }

	const auto drawChain = [&](
		const std::array<physics::BodyHandle, MagnetChainSystem::kLinksPerSide>& handles,
		std::size_t count, uint32_t sideSeed) {
		Vector3 previous = player->position;
		for (std::size_t link = 0; link < count; ++link) {
			const auto* body = chain.GetPhysicsWorld().GetBody(handles[link]);
			if (!body || !body->active) { continue; }
			const Vector3 delta = body->position - previous;
			const Vector3 horizontalSide = NormalizeOr({ -delta.z, 0.0f, delta.x }, { 1.0f, 0.0f, 0.0f });
			const float jitter = 0.035f + charge * 0.16f;
			std::array<Vector3, kArcSegments + 1> points{};
			points.front() = previous;
			points.back() = body->position;
			for (int segment = 1; segment < kArcSegments; ++segment) {
				const float t = static_cast<float>(segment) / kArcSegments;
				const uint32_t key = phase * 977u + sideSeed +
					static_cast<uint32_t>(link * 131u + segment * 37u);
				points[segment] = previous + delta * t +
					horizontalSide * (SignedNoise(key) * jitter) +
					Vector3{ 0.0f, SignedNoise(key + 19u) * jitter * 0.65f, 0.0f };
			}
			for (int segment = 0; segment < kArcSegments; ++segment) {
				const Vector4 glow{ 0.22f + charge * 0.25f, 0.35f, 1.0f, 0.45f + charge * 0.35f };
				const Vector4 core{ 0.78f, 0.92f, 1.0f, 0.9f };
				const Vector3 offset = horizontalSide * (0.012f + charge * 0.018f);
				lineDrawer.DrawLine(points[segment] + offset, points[segment + 1] + offset, glow);
				lineDrawer.DrawLine(points[segment] - offset, points[segment + 1] - offset, glow);
				lineDrawer.DrawLine(points[segment], points[segment + 1], core);
			}

			const float pulse = 0.5f + 0.5f * std::sin(
				elapsedSeconds_ * (10.0f + charge * 9.0f) + static_cast<float>(link));
			const float haloRadius = body->radius * (1.12f + charge * 0.55f + pulse * 0.12f);
			for (int segment = 0; segment < kHaloSegments; ++segment) {
				const float a = kTwoPi * static_cast<float>(segment) / kHaloSegments;
				const float b = kTwoPi * static_cast<float>(segment + 1) / kHaloSegments;
				lineDrawer.DrawLine(
					body->position + Vector3{ std::cos(a) * haloRadius, 0.04f, std::sin(a) * haloRadius },
					body->position + Vector3{ std::cos(b) * haloRadius, 0.04f, std::sin(b) * haloRadius },
					{ 0.35f, 0.62f + charge * 0.28f, 1.0f, 0.45f + pulse * 0.35f });
			}

			const int sparkCount = 2 + static_cast<int>(charge * 5.0f);
			const float sparkLength = 0.12f + charge * 0.42f;
			for (int spark = 0; spark < sparkCount; ++spark) {
				const uint32_t key = phase * 313u + sideSeed +
					static_cast<uint32_t>(link * 71u + spark * 17u);
				const float angle = kTwoPi * static_cast<float>(spark) / sparkCount + SignedNoise(key) * 0.3f;
				const Vector3 direction = NormalizeOr(
					{ std::cos(angle), 0.25f + SignedNoise(key + 5u) * 0.3f, std::sin(angle) },
					{ 1.0f, 0.0f, 0.0f });
				lineDrawer.DrawLine(body->position + direction * (body->radius * 0.9f),
					body->position + direction * (body->radius + sparkLength),
					{ 0.72f, 0.88f, 1.0f, 0.75f + charge * 0.25f });
			}
			previous = body->position;
		}
	};

	drawChain(chain.GetLeftChain(), chain.GetLeftChainCount(), 0x1451u);
	drawChain(chain.GetRightChain(), chain.GetRightChainCount(), 0x9e37u);
}
} // namespace magnet
