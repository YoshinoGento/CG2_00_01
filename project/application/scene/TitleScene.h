#pragma once
#include "BaseScene.h"

/**
 * TitleScene
 * ゲームの開始待機画面。
 */
class TitleScene : public BaseScene {
public:
	void Initialize() override;
	void Finalize() override;
	void Update() override;
	void Draw() override;
};