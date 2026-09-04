#pragma once

#include "math/Matrix.h"
#include "math/Struct.h"
#include "2d/Sprite.h"
#include "2d/BitmapFont.h"
#include "2d/SpriteText.h"

#include <memory>
#include <string>
#include <vector>

class SpriteCommon;

struct ComicTextEffectPreset {
	std::string text = "BOOOM!!";
	bool useEditableText = true;
	std::string texturePath = "Resources/effects/comic/booom_impact.png";
	float textScale = 1.25f;
	float characterSpacing = -14.0f;
	Vector2 extrusionOffset = { 8.0f, 10.0f };
	Vector4 textColor = { 1.0f, 0.72f, 0.08f, 1.0f };
	Vector4 extrusionColor = { 0.95f, 0.08f, 0.02f, 1.0f };
	Vector2 size = { 520.0f, 278.0f };
	Vector2 screenOffset = { 0.0f, -90.0f };
	Vector2 drift = { 0.0f, -30.0f };
	Vector4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
	float duration = 0.72f;
	float startScale = 0.12f;
	float peakScale = 1.16f;
	float endScale = 0.88f;
	float popFraction = 0.22f;
	float fadeFraction = 0.35f;
	float rotation = -0.08f;
	float shakeAmplitude = 7.0f;
	float shakeFrequency = 34.0f;
};

// 3D上の出来事へ追従する、漫画風の2D擬音スプライトを再生する。
class ComicTextEffectSystem final {
public:
	bool Initialize(SpriteCommon* spriteCommon);
	void Clear();
	bool Play(const ComicTextEffectPreset& preset, const Vector3& worldPosition);
	bool Play(const std::string& presetName, const Vector3& worldPosition);
	void Update(float deltaTime, const Matrix4x4& viewProjection);
	void Draw();

	static bool SavePreset(const std::string& presetName, const ComicTextEffectPreset& preset);
	static bool LoadPreset(const std::string& presetName, ComicTextEffectPreset& preset);

private:
	struct ActiveEffect {
		ComicTextEffectPreset preset;
		Vector3 worldPosition{};
		std::unique_ptr<Sprite> sprite;
		std::unique_ptr<SpriteText> extrusionText;
		std::unique_ptr<SpriteText> mainText;
		float elapsed = 0.0f;
		bool visible = false;
	};

	static bool IsSafePresetName(const std::string& presetName);
	static std::string BuildPresetPath(const std::string& presetName);

	SpriteCommon* spriteCommon_ = nullptr;
	std::unique_ptr<BitmapFont> comicFont_;
	std::vector<ActiveEffect> effects_;
};
