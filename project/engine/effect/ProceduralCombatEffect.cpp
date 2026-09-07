#include "effect/ProceduralCombatEffect.h"

#include "3d/LineDrawer.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr float kPi = 3.14159265358979323846f;

float ClampFinite(float value, float minimum, float maximum, float fallback) {
	return std::isfinite(value) ? std::clamp(value, minimum, maximum) : fallback;
}

Vector3 Normalize(const Vector3& value) {
	const float lengthSquared = value.x * value.x + value.y * value.y + value.z * value.z;
	if (lengthSquared <= 0.000001f || !std::isfinite(lengthSquared)) return { 1.0f, 0.0f, 0.0f };
	return value / std::sqrt(lengthSquared);
}

uint32_t Hash(uint32_t value) {
	value ^= value >> 16;
	value *= 0x7feb352du;
	value ^= value >> 15;
	value *= 0x846ca68bu;
	return value ^ (value >> 16);
}

float SignedNoise(uint32_t seed) {
	return static_cast<float>(Hash(seed) & 0xffffu) / 32767.5f - 1.0f;
}

Vector4 FadeColor(Vector4 color, float alpha) {
	color.w *= std::clamp(alpha, 0.0f, 1.0f);
	color.x *= std::clamp(alpha, 0.18f, 1.0f);
	color.y *= std::clamp(alpha, 0.18f, 1.0f);
	color.z *= std::clamp(alpha, 0.18f, 1.0f);
	return color;
}

void DrawThickSegment(LineDrawer& lines, const Vector3& start, const Vector3& end,
	float width, const Vector4& glow, const Vector4& core) {
	const Vector3 direction = Normalize(end - start);
	Vector3 side = Normalize({ -direction.y, direction.x, 0.0f });
	if (std::abs(direction.x) + std::abs(direction.y) < 0.01f) side = { 1.0f, 0.0f, 0.0f };
	const float outer = width;
	lines.DrawLine(start + side * outer, end + side * outer, glow);
	lines.DrawLine(start - side * outer, end - side * outer, glow);
	lines.DrawLine(start + side * (outer * 0.45f), end + side * (outer * 0.45f), core);
	lines.DrawLine(start - side * (outer * 0.45f), end - side * (outer * 0.45f), core);
	lines.DrawLine(start, end, core);
}
}

void LightningEffect::Play(const LightningEffectSettings& requested, const Vector3& origin) {
	LightningEffectSettings settings = requested;
	settings.direction = Normalize(settings.direction);
	settings.length = ClampFinite(settings.length, 0.1f, 100.0f, 5.0f);
	settings.size = ClampFinite(settings.size, 0.05f, 20.0f, 1.0f);
	settings.width = ClampFinite(settings.width, 0.001f, 3.0f, 0.12f);
	settings.jaggedness = ClampFinite(settings.jaggedness, 0.0f, 10.0f, 0.55f);
	settings.segments = std::clamp(settings.segments, 2, 128);
	settings.boltCount = std::clamp(settings.boltCount, 1, 16);
	settings.spreadDegrees = ClampFinite(settings.spreadDegrees, 0.0f, 360.0f, 300.0f);
	settings.branches = std::clamp(settings.branches, 0, 32);
	settings.branchLength = ClampFinite(settings.branchLength, 0.05f, 1.0f, 0.32f);
	settings.flickerSpeed = ClampFinite(settings.flickerSpeed, 0.0f, 120.0f, 24.0f);
	settings.duration = ClampFinite(settings.duration, 0.03f, 10.0f, 0.45f);
	active_.push_back({ settings, origin, 0.0f });
}

void LightningEffect::Update(float deltaTime) {
	deltaTime = ClampFinite(deltaTime, 0.0f, 0.1f, 0.0f);
	for (ActiveLightning& effect : active_) effect.elapsed += deltaTime;
	std::erase_if(active_, [](const ActiveLightning& effect) {
		return effect.elapsed >= effect.settings.duration;
	});
}

void LightningEffect::Draw(LineDrawer& lines) const {
	for (const ActiveLightning& effect : active_) {
		const auto& s = effect.settings;
		const float life = 1.0f - effect.elapsed / s.duration;
		const uint32_t phase = static_cast<uint32_t>(effect.elapsed * s.flickerSpeed);
		const Vector3 baseDirection = Normalize(s.direction);
		const float baseAngle = std::atan2(baseDirection.y, baseDirection.x);
		const float spread = s.spreadDegrees * kPi / 180.0f;
		for (int bolt = 0; bolt < s.boltCount; ++bolt) {
			const uint32_t boltKey = s.randomSeed + phase * 313u + static_cast<uint32_t>(bolt * 1543);
			const float ratio = s.boltCount <= 1 ? 0.5f : static_cast<float>(bolt) / static_cast<float>(s.boltCount - 1);
			const float angle = baseAngle + (ratio - 0.5f) * spread + SignedNoise(boltKey) * 0.18f;
			const Vector3 direction = Normalize({ std::cos(angle), std::sin(angle), SignedNoise(boltKey + 7u) * 0.22f });
			const Vector3 side = Normalize({ -direction.y, direction.x, 0.0f });
			std::vector<Vector3> points(static_cast<size_t>(s.segments) + 1u);
			const float boltLength = s.length * s.size * (0.68f + 0.32f * (SignedNoise(boltKey + 11u) * 0.5f + 0.5f));
			for (int index = 0; index <= s.segments; ++index) {
				const float t = static_cast<float>(index) / static_cast<float>(s.segments);
				Vector3 point = effect.origin + direction * (boltLength * t);
				if (index != 0 && index != s.segments) {
					const uint32_t key = boltKey + static_cast<uint32_t>(index * 977);
					point += side * (SignedNoise(key) * s.jaggedness * s.size * std::sin(t * kPi));
				}
				points[static_cast<size_t>(index)] = point;
			}
			for (int index = 0; index < s.segments; ++index) {
				DrawThickSegment(lines, points[index], points[index + 1], s.width * s.size,
					FadeColor(s.glowColor, life), FadeColor(s.coreColor, life));
			}
			for (int branch = 0; branch < s.branches; ++branch) {
				const int pointIndex = 1 + static_cast<int>(Hash(boltKey + branch * 59u) % static_cast<uint32_t>(s.segments - 1));
				const Vector3 start = points[pointIndex];
				const Vector3 end = start + Normalize(direction * 0.25f + side * SignedNoise(boltKey + branch * 83u)) *
					(boltLength * s.branchLength);
				lines.DrawLine(start, end, FadeColor(s.coreColor, life));
			}
		}
	}
}

void SlashEffect::Play(const SlashEffectSettings& requested, const Vector3& origin) {
	SlashEffectSettings settings = requested;
	settings.size = ClampFinite(settings.size, 0.05f, 20.0f, 1.0f);
	settings.radius = ClampFinite(settings.radius, 0.1f, 50.0f, 3.0f);
	settings.arcDegrees = ClampFinite(settings.arcDegrees, 5.0f, 355.0f, 125.0f);
	settings.rotationDegrees = ClampFinite(settings.rotationDegrees, -720.0f, 720.0f, 20.0f);
	settings.thickness = ClampFinite(settings.thickness, 0.001f, 5.0f, 0.3f);
	settings.segments = std::clamp(settings.segments, 3, 160);
	settings.afterimages = std::clamp(settings.afterimages, 0, 16);
	settings.afterimageSpacingDegrees = ClampFinite(settings.afterimageSpacingDegrees, 0.0f, 45.0f, 5.0f);
	settings.duration = ClampFinite(settings.duration, 0.03f, 10.0f, 0.5f);
	active_.push_back({ settings, origin, 0.0f });
}

void SlashEffect::Update(float deltaTime) {
	deltaTime = ClampFinite(deltaTime, 0.0f, 0.1f, 0.0f);
	for (ActiveSlash& effect : active_) effect.elapsed += deltaTime;
	std::erase_if(active_, [](const ActiveSlash& effect) {
		return effect.elapsed >= effect.settings.duration;
	});
}

void SlashEffect::Draw(LineDrawer& lines) const {
	for (const ActiveSlash& effect : active_) {
		const auto& s = effect.settings;
		const float progress = std::clamp(effect.elapsed / s.duration, 0.0f, 1.0f);
		const float life = 1.0f - progress;
		const float radius = s.radius * s.size;
		const float curvature = s.arcDegrees * kPi / 180.0f;
		const float baseRotation = s.rotationDegrees * kPi / 180.0f;
		const int cutCount = s.afterimages + 1;
		for (int cut = 0; cut < cutCount; ++cut) {
			const float cutFade = 1.0f - static_cast<float>(cut) / static_cast<float>(cutCount + 1);
			const float rotation = baseRotation + cut * s.afterimageSpacingDegrees * kPi / 180.0f;
			const Vector4 outer = FadeColor(s.outerColor, life * cutFade);
			const Vector4 core = FadeColor(s.coreColor, life * cutFade);
			for (int index = 0; index < s.segments; ++index) {
				const float t0 = static_cast<float>(index) / static_cast<float>(s.segments) - 0.5f;
				const float t1 = static_cast<float>(index + 1) / static_cast<float>(s.segments) - 0.5f;
				const auto makePoint = [&](float t) {
					const float along = t * radius * 2.0f * (0.65f + 0.35f * progress);
					const float bend = std::sin((t + 0.5f) * kPi) * curvature * radius * 0.16f;
					return effect.origin + Vector3{
						std::cos(rotation) * along - std::sin(rotation) * bend,
						std::sin(rotation) * along + std::cos(rotation) * bend,
						static_cast<float>(cut) * 0.01f };
				};
				DrawThickSegment(lines, makePoint(t0), makePoint(t1), s.thickness * s.size * cutFade, outer, core);
			}
		}
	}
}
