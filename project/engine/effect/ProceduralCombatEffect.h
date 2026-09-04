#pragma once

#include "math/Struct.h"

#include <cstdint>
#include <vector>

class LineDrawer;

struct LightningEffectSettings {
	Vector3 direction{ 1.0f, 0.15f, 0.0f };
	float length = 5.0f;
	float size = 1.0f;
	float width = 0.12f;
	float jaggedness = 0.55f;
	int segments = 18;
	int branches = 5;
	float branchLength = 0.32f;
	float flickerSpeed = 24.0f;
	float duration = 0.45f;
	Vector4 glowColor{ 0.18f, 0.45f, 1.0f, 0.65f };
	Vector4 coreColor{ 0.85f, 0.95f, 1.0f, 1.0f };
	uint32_t randomSeed = 1;
};

class LightningEffect final {
public:
	void Play(const LightningEffectSettings& settings, const Vector3& origin);
	void Update(float deltaTime);
	void Draw(LineDrawer& lineDrawer) const;
	void Clear() noexcept { active_.clear(); }

private:
	struct ActiveLightning {
		LightningEffectSettings settings{};
		Vector3 origin{};
		float elapsed = 0.0f;
	};
	std::vector<ActiveLightning> active_;
};

struct SlashEffectSettings {
	float size = 1.0f;
	float radius = 3.0f;
	float arcDegrees = 125.0f;
	float rotationDegrees = 20.0f;
	float thickness = 0.3f;
	int segments = 28;
	int afterimages = 4;
	float afterimageSpacingDegrees = 5.0f;
	float duration = 0.5f;
	Vector4 outerColor{ 0.15f, 0.65f, 1.0f, 0.7f };
	Vector4 coreColor{ 0.9f, 1.0f, 1.0f, 1.0f };
};

class SlashEffect final {
public:
	void Play(const SlashEffectSettings& settings, const Vector3& origin);
	void Update(float deltaTime);
	void Draw(LineDrawer& lineDrawer) const;
	void Clear() noexcept { active_.clear(); }

private:
	struct ActiveSlash {
		SlashEffectSettings settings{};
		Vector3 origin{};
		float elapsed = 0.0f;
	};
	std::vector<ActiveSlash> active_;
};
