#pragma once

#include "2d/BitmapFont.h"
#include "2d/Sprite.h"
#include "2d/SpriteText.h"

#include <cstdint>
#include <string>

class SpriteCommon;

struct PostEffectSubmissionHUDViewData {
	std::string effectName;
	bool showDissolveProgress = false;
	uint32_t dissolvePercent = 0;
};

// Release-only presentation view. It never selects or mutates a PostEffect.
class PostEffectSubmissionHUD final {
public:
	bool Initialize(SpriteCommon* spriteCommon);
	void SetViewData(const PostEffectSubmissionHUDViewData& viewData);
	void Update();
	void Draw();

private:
	void RefreshText();

	PostEffectSubmissionHUDViewData viewData_{};
	BitmapFont font_;
	Sprite background_;
	SpriteText effectShadowText_;
	SpriteText effectText_;
	SpriteText shortcutText_;
	bool initialized_ = false;
};
