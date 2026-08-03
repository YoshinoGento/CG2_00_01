#include "application/level/ui/StageClearHUD.h"

#include "2d/SpriteCommon.h"
#include "base/Logger.h"

#include <string>

namespace {
constexpr const char* kFontConfigPath = "Resources/ui/font/ascii_bitmap_font.json";
constexpr const char* kStageClearText = "STAGE CLEAR";
constexpr const char* kRetryText = "F5 / CTRL+R : RETRY";
constexpr Vector2 kTitlePosition{ 328.0f, 270.0f };
constexpr Vector2 kRetryPosition{ 438.0f, 360.0f };
constexpr Vector2 kShadowOffset{ 4.0f, 4.0f };
constexpr float kTitleScale = 2.0f;
constexpr float kTitleSpacing = -8.0f;
constexpr float kRetryScale = 0.75f;
constexpr float kRetrySpacing = -4.0f;
constexpr Vector4 kTitleColor{ 1.0f, 0.86f, 0.18f, 1.0f };
constexpr Vector4 kRetryColor{ 1.0f, 1.0f, 1.0f, 1.0f };
constexpr Vector4 kShadowColor{ 0.0f, 0.0f, 0.0f, 0.85f };

Vector2 WithShadowOffset(const Vector2& position)
{
	return { position.x + kShadowOffset.x, position.y + kShadowOffset.y };
}

void ConfigureText(
	SpriteText& text,
	const std::string& value,
	const Vector2& position,
	float scale,
	float spacing,
	const Vector4& color)
{
	text.SetText(value);
	text.SetPosition(position);
	text.SetScale(scale);
	text.SetCharacterSpacing(spacing);
	text.SetColor(color);
}
}

bool StageClearHUD::Initialize(SpriteCommon* spriteCommon)
{
	if (spriteCommon == nullptr) {
		Logger::Log("StageClearHUD::Initialize failed. spriteCommon is null.");
		return false;
	}
	if (!font_.InitializeFromJson(spriteCommon, kFontConfigPath)) {
		Logger::Log("StageClearHUD::Initialize failed. font initialization failed.");
		return false;
	}

	titleShadowText_.Initialize(spriteCommon, &font_);
	titleText_.Initialize(spriteCommon, &font_);
	retryShadowText_.Initialize(spriteCommon, &font_);
	retryText_.Initialize(spriteCommon, &font_);
	ConfigureText(titleShadowText_, kStageClearText, WithShadowOffset(kTitlePosition), kTitleScale, kTitleSpacing, kShadowColor);
	ConfigureText(titleText_, kStageClearText, kTitlePosition, kTitleScale, kTitleSpacing, kTitleColor);
	ConfigureText(retryShadowText_, kRetryText, WithShadowOffset(kRetryPosition), kRetryScale, kRetrySpacing, kShadowColor);
	ConfigureText(retryText_, kRetryText, kRetryPosition, kRetryScale, kRetrySpacing, kRetryColor);
	initialized_ = true;
	return true;
}

void StageClearHUD::Update()
{
	if (!initialized_ || !visible_) {
		return;
	}
	titleShadowText_.Update();
	titleText_.Update();
	retryShadowText_.Update();
	retryText_.Update();
}

void StageClearHUD::Draw()
{
	if (!initialized_ || !visible_) {
		return;
	}
	titleShadowText_.Draw();
	titleText_.Draw();
	retryShadowText_.Draw();
	retryText_.Draw();
}
