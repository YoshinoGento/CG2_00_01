#pragma once

#include "BaseScene.h"
#include "title/TitlePresentationSystem.h"
#include "title/TitleFarmRenderer.h"
#include "title/TitleView.h"

class TitleScene : public BaseScene {
public:
	void Initialize() override;
	void Finalize() override;
	void Update() override;
	void Draw() override;
	void RequestStart() noexcept { presentation_.RequestStart(); }

private:
	title::TitlePresentationSystem presentation_;
	title::TitleFarmRenderer renderer_;
	title::TitleView view_;
	bool ready_ = false;
};
