#pragma once

#include "BaseScene.h"
#include "title/TitlePresentationSystem.h"
#include "title/TitleFarmRenderer.h"
#include "title/TitleView.h"
#include "title/TitleAudioSystem.h"
#include "title/TitleAudioSettingsSystem.h"
#include "title/TitleAudioSettingsView.h"
#include "title/TitlePondRenderer.h"

class TitleScene : public BaseScene {
public:
	void Initialize() override;
    InitializationTask InitializeIncrementally() override;
	void Finalize() override;
	void Update() override;
	void UpdateTransitionPresentation() override { Update(); }
	void Draw() override;
    void RequestStart() noexcept;
	void SetSettingsViewportInput(const title::AudioSettingsInput& input, bool focused) noexcept {
		viewportInput_ = input; viewportFocused_ = focused;
	}

private:
	title::TitlePresentationSystem presentation_;
	title::TitleFarmRenderer renderer_;
	title::TitleView view_;
	title::TitleAudioSystem audio_;
	title::TitleAudioSettingsSystem settings_;
	title::TitleAudioSettingsView settingsView_;
    title::TitlePondSystem pond_;
    title::TitlePondRenderer pondRenderer_;
	title::AudioSettingsInput viewportInput_{};
	bool viewportFocused_ = false;
	bool ready_ = false;
    bool pondReady_ = false;
};
