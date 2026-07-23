#include "demo/PostEffectSubmissionHUD.h"

#include "2d/SpriteCommon.h"
#include "base/Logger.h"

#include <algorithm>

namespace {
constexpr const char* kFontConfigPath = "Resources/ui/font/ascii_bitmap_font.json";
constexpr const char* kPanelTexturePath = "Resources/human/white.png";
constexpr Vector2 kPanelPosition{ 140.0f, 54.0f };
constexpr Vector2 kPanelSize{ 1000.0f, 96.0f };
constexpr Vector2 kEffectPosition{ 176.0f, 72.0f };
constexpr Vector2 kShortcutPosition{ 834.0f, 120.0f };
constexpr Vector2 kShadowOffset{ 3.0f, 3.0f };
constexpr float kEffectScale = 1.20f;
constexpr float kEffectSpacing = -8.0f;
constexpr float kShortcutScale = 0.80f;
constexpr float kShortcutSpacing = -2.0f;
constexpr Vector4 kEffectColor{ 1.0f, 0.86f, 0.18f, 1.0f };
constexpr Vector4 kShortcutColor{ 0.72f, 0.92f, 1.0f, 1.0f };
constexpr Vector4 kShadowColor{ 0.0f, 0.0f, 0.0f, 0.85f };
constexpr Vector4 kPanelColor{ 0.01f, 0.02f, 0.03f, 0.72f };

Vector2 WithShadow(const Vector2& position) {
	return { position.x + kShadowOffset.x, position.y + kShadowOffset.y };
}

void ConfigureText(
	SpriteText& text,
	const Vector2& position,
	float scale,
	float spacing,
	const Vector4& color) {
	text.SetPosition(position);
	text.SetScale(scale);
	text.SetCharacterSpacing(spacing);
	text.SetColor(color);
}
}

bool PostEffectSubmissionHUD::Initialize(SpriteCommon* spriteCommon) {
	if (spriteCommon == nullptr) {
		Logger::Log("PostEffectSubmissionHUD::Initialize failed. SpriteCommon is null.");
		return false;
	}
	if (!font_.InitializeFromJson(spriteCommon, kFontConfigPath)) {
		Logger::Log("PostEffectSubmissionHUD::Initialize failed. BitmapFont is unavailable.");
		return false;
	}
	if (!background_.Initialize(spriteCommon, kPanelTexturePath)) {
		Logger::Log("PostEffectSubmissionHUD::Initialize failed. Panel texture is unavailable.");
		return false;
	}

	effectShadowText_.Initialize(spriteCommon, &font_);
	effectText_.Initialize(spriteCommon, &font_);
	shortcutText_.Initialize(spriteCommon, &font_);

	background_.SetPosition(kPanelPosition);
	background_.SetSize(kPanelSize);
	background_.SetColor(kPanelColor);
	ConfigureText(effectShadowText_, WithShadow(kEffectPosition), kEffectScale, kEffectSpacing, kShadowColor);
	ConfigureText(effectText_, kEffectPosition, kEffectScale, kEffectSpacing, kEffectColor);
	ConfigureText(shortcutText_, kShortcutPosition, kShortcutScale, kShortcutSpacing, kShortcutColor);

	// Prewarm the bounded glyph pool so percentage updates do not allocate Sprite resources.
	effectShadowText_.SetText("POST EFFECT  LUMINANCE OUTLINE  100%");
	effectText_.SetText("POST EFFECT  LUMINANCE OUTLINE  100%");
	shortcutText_.SetText("SPACE : NEXT");

	initialized_ = true;
	RefreshText();
	return true;
}

void PostEffectSubmissionHUD::SetViewData(
	const PostEffectSubmissionHUDViewData& viewData) {
	if (viewData_.effectName == viewData.effectName &&
		viewData_.showDissolveProgress == viewData.showDissolveProgress &&
		viewData_.dissolvePercent == viewData.dissolvePercent) {
		return;
	}

	viewData_ = viewData;
	viewData_.dissolvePercent = (std::min)(viewData_.dissolvePercent, 100u);
	RefreshText();
}

void PostEffectSubmissionHUD::Update() {
	if (!initialized_) {
		return;
	}
	background_.Update();
	effectShadowText_.Update();
	effectText_.Update();
	shortcutText_.Update();
}

void PostEffectSubmissionHUD::Draw() {
	if (!initialized_) {
		return;
	}
	background_.Draw();
	effectShadowText_.Draw();
	effectText_.Draw();
	shortcutText_.Draw();
}

void PostEffectSubmissionHUD::RefreshText() {
	if (!initialized_) {
		return;
	}

	std::string effectText = "POST EFFECT  " + viewData_.effectName;
	if (viewData_.showDissolveProgress) {
		effectText += "  " + std::to_string(viewData_.dissolvePercent) + "%";
	}

	effectShadowText_.SetText(effectText);
	effectText_.SetText(effectText);
}
