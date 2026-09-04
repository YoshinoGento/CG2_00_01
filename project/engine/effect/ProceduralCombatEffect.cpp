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
		const Vector3 direction = Normalize(s.direction);
		Vector3 side = Normalize({ -direction.y, direction.x, 0.0f });
		if (std::abs(direction.x) + std::abs(direction.y) < 0.01f) side = { 1.0f, 0.0f, 0.0f };
		const Vector3 depth = Normalize({ -direction.z, 0.0f, direction.x });
		std::vector<Vector3> points(static_cast<size_t>(s.segments) + 1u);
		for (int index = 0; index <= s.segments; ++index) {
			const float t = static_cast<float>(index) / static_cast<float>(s.segments);
			Vector3 point = effect.origin + direction * (s.length * s.size * t);
			if (index != 0 && index != s.segments) {
				const float envelope = std::sin(t * kPi);
				const uint32_t key = s.randomSeed + static_cast<uint32_t>(index * 977) + phase * 131u;
				point += side * (SignedNoise(key) * s.jaggedness * s.size * envelope);
				point += depth * (SignedNoise(key + 47u) * s.jaggedness * s.size * 0.45f * envelope);
			}
			points[static_cast<size_t>(index)] = point;
		}
		for (int index = 0; index < s.segments; ++index) {
			DrawThickSegment(lines, points[index], points[index + 1], s.width * s.size,
				FadeColor(s.glowColor, life), FadeColor(s.coreColor, life));
		}
		for (int branch = 0; branch < s.branches; ++branch) {
			const uint32_t key = s.randomSeed + phase * 313u + static_cast<uint32_t>(branch * 1543);
			const int pointIndex = 1 + static_cast<int>(Hash(key) % static_cast<uint32_t>(s.segments - 1));
			const float sign = SignedNoise(key + 9u) < 0.0f ? -1.0f : 1.0f;
			const Vector3 branchDirection = Normalize(direction * 0.45f + side * sign + depth * SignedNoise(key + 21u));
			const Vector3 start = points[pointIndex];
			const Vector3 middle = start + branchDirection * (s.length * s.size * s.branchLength * 0.55f) +
				side * (SignedNoise(key + 67u) * s.jaggedness * s.size);
			const Vector3 end = start + branchDirection * (s.length * s.size * s.branchLength);
			DrawThickSegment(lines, start, middle, s.width * s.size * 0.55f,
				FadeColor(s.glowColor, life), FadeColor(s.coreColor, life));
			DrawThickSegment(lines, middle, end, s.width * s.size * 0.35f,
				FadeColor(s.glowColor, life), FadeColor(s.coreColor, life));
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
		const float arc = s.arcDegrees * kPi / 180.0f;
		const float baseRotation = s.rotationDegrees * kPi / 180.0f + progress * 0.3f;
		for (int trail = s.afterimages; trail >= 0; --trail) {
			const float trailFade = 1.0f - static_cast<float>(trail) / static_cast<float>(s.afterimages + 1);
			const float rotation = baseRotation - trail * s.afterimageSpacingDegrees * kPi / 180.0f;
			const Vector4 outer = FadeColor(s.outerColor, life * trailFade);
			const Vector4 core = FadeColor(s.coreColor, life * trailFade);
			for (int index = 0; index < s.segments; ++index) {
				const float t0 = static_cast<float>(index) / static_cast<float>(s.segments);
				const float t1 = static_cast<float>(index + 1) / static_cast<float>(s.segments);
				const float a0 = rotation - arc * 0.5f + arc * t0;
				const float a1 = rotation - arc * 0.5f + arc * t1;
				const Vector3 p0 = effect.origin + Vector3{ std::cos(a0) * radius, std::sin(a0) * radius, 0.0f };
				const Vector3 p1 = effect.origin + Vector3{ std::cos(a1) * radius, std::sin(a1) * radius, 0.0f };
				DrawThickSegment(lines, p0, p1, s.thickness * s.size * trailFade, outer, core);
			}
		}
	}
}
