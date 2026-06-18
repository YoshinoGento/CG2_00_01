#pragma once

#include "math/Matrix.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class Sprite;
class SpriteCommon;

// Screen UI: HP, money counters, menus, and buttons.
// World Projected UI: reward popups, damage numbers, crop labels, and speech bubbles.
// World Effect: GPU particles, dust, lights, and impact effects.
// FloatingTextSystem owns World Projected UI; entries keep world positions and are
// projected to screen/virtual-render coordinates every frame.
class FloatingTextSystem {
public:
	static constexpr uint32_t kDefaultPoolSize = 32;

	void Initialize(SpriteCommon* spriteCommon, const std::string& texturePath, uint32_t poolSize = kDefaultPoolSize);
	void Update(float deltaTime);
	void Draw(const Vector2& viewportTopLeft, const Vector2& viewportSize, const Matrix4x4* viewProjection);

	void PlayRewardPopup(const Vector3& worldPosition, int32_t price);

	bool IsReady() const { return ready_; }

private:
	struct FloatingText {
		bool active = false;
		Vector3 worldPosition = { 0.0f, 0.0f, 0.0f };
		Vector3 velocity = { 0.0f, 0.8f, 0.0f };
		float timer = 0.0f;
		float duration = 1.0f;
		float alpha = 1.0f;
		float scale = 1.0f;
		Vector2 baseSize = { 180.0f, 64.0f };
		std::unique_ptr<Sprite> sprite;
	};

	FloatingText* AllocateFloatingText();
	static bool ProjectWorldToViewport(
		const Vector3& worldPosition,
		const Matrix4x4& viewProjection,
		const Vector2& viewportTopLeft,
		const Vector2& viewportSize,
		Vector2& outScreenPosition);

	SpriteCommon* spriteCommon_ = nullptr;
	uint32_t textureHandle_ = 0;
	Vector2 textureBaseSize_ = { 180.0f, 64.0f };
	std::vector<FloatingText> floatingTexts_;
	bool ready_ = false;
};
