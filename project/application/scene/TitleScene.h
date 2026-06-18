#pragma once

#include "BaseScene.h"
#include "2d/Sprite.h"

#include <memory>

class SpriteCommon;

class TitleScene : public BaseScene {
public:
	void Initialize() override;
	void Finalize() override;
	void Update() override;
	void Draw() override;

private:
	SpriteCommon* spriteCommon_ = nullptr;
	std::unique_ptr<Sprite> backgroundSprite_;
	std::unique_ptr<Sprite> titleLogoSprite_;
	std::unique_ptr<Sprite> pressSpaceSprite_;
};
