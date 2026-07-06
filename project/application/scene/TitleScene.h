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
};
