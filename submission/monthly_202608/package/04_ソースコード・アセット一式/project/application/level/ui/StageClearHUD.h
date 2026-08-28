#pragma once

#include "2d/BitmapFont.h"
#include "2d/SpriteText.h"

class SpriteCommon;

// Presentation-only overlay for the level completion state.
class StageClearHUD {
public:
	bool Initialize(SpriteCommon* spriteCommon);
	void SetVisible(bool visible) { visible_ = visible; }
	void Update();
	void Draw();

private:
	BitmapFont font_;
	SpriteText titleShadowText_;
	SpriteText titleText_;
	SpriteText retryShadowText_;
	SpriteText retryText_;
	bool initialized_ = false;
	bool visible_ = false;
};
