#include "demo/PostEffectSubmissionDemo.h"

#include "base/Logger.h"
#include "base/WinApp.h"
#include "effect/PostEffectSystem.h"
#include "io/Input.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

namespace {
constexpr float kDissolvePlaybackSeconds = 4.0f;

struct SubmissionPreset {
	PostEffectSystem::DemoPreset preset;
	const char* name;
};

constexpr std::array kSubmissionPresets = {
	SubmissionPreset{ PostEffectSystem::DemoPreset::Original, "Original Color" },
	SubmissionPreset{ PostEffectSystem::DemoPreset::RequiredGrayscale, "Grayscale" },
	SubmissionPreset{ PostEffectSystem::DemoPreset::Vignette, "Vignette" },
	SubmissionPreset{ PostEffectSystem::DemoPreset::BoxFilter, "BoxFilter 3x3" },
	SubmissionPreset{ PostEffectSystem::DemoPreset::GaussianFilter, "GaussianFilter 5x5" },
	SubmissionPreset{ PostEffectSystem::DemoPreset::LuminanceBasedOutline, "Luminance Outline" },
	SubmissionPreset{ PostEffectSystem::DemoPreset::DepthBasedOutline, "Depth Outline" },
	SubmissionPreset{ PostEffectSystem::DemoPreset::RadialBlur, "RadialBlur" },
	SubmissionPreset{ PostEffectSystem::DemoPreset::Dissolve, "Dissolve" },
	SubmissionPreset{ PostEffectSystem::DemoPreset::RandomNoise, "RandomNoise" },
};
}

void PostEffectSubmissionDemo::Initialize(
	PostEffectSystem& postEffectSystem,
	WinApp& winApp) {
	presetIndex_ = 0;
	ApplyCurrentPreset(postEffectSystem, winApp);
}

void PostEffectSubmissionDemo::Update(
	float deltaTime,
	Input& input,
	PostEffectSystem& postEffectSystem,
	WinApp& winApp) {
	if (input.ConsumeTriggerKey(InputKey::Space)) {
		presetIndex_ = (presetIndex_ + 1) % kSubmissionPresets.size();
		ApplyCurrentPreset(postEffectSystem, winApp);
		return;
	}

	UpdateDissolve(deltaTime, postEffectSystem, winApp);
}

void PostEffectSubmissionDemo::ApplyCurrentPreset(
	PostEffectSystem& postEffectSystem,
	WinApp& winApp) {
	const SubmissionPreset& selected = kSubmissionPresets[presetIndex_];
	postEffectSystem.ApplyDemoPreset(selected.preset);

	dissolveElapsedSeconds_ = 0.0f;
	dissolvePercent_ = 0;
	if (IsDissolveActive()) {
		postEffectSystem.SetDissolveThreshold(0.0f);
	}

	UpdateWindowTitle(winApp);
	Logger::Info(std::string("[SubmissionDemo] PostEffect: ") + selected.name);
}

void PostEffectSubmissionDemo::UpdateDissolve(
	float deltaTime,
	PostEffectSystem& postEffectSystem,
	WinApp& winApp) {
	if (!IsDissolveActive() || dissolvePercent_ >= 100) {
		return;
	}

	if (!std::isfinite(deltaTime)) {
		deltaTime = 0.0f;
	}
	dissolveElapsedSeconds_ = (std::min)(
		dissolveElapsedSeconds_ + std::clamp(deltaTime, 0.0f, 0.1f),
		kDissolvePlaybackSeconds);

	const float progress = dissolveElapsedSeconds_ / kDissolvePlaybackSeconds;
	postEffectSystem.SetDissolveThreshold(progress);

	const uint32_t nextPercent = static_cast<uint32_t>(
		std::clamp(std::lround(progress * 100.0f), 0l, 100l));
	if (nextPercent != dissolvePercent_) {
		dissolvePercent_ = nextPercent;
		UpdateWindowTitle(winApp);
	}
}

const char* PostEffectSubmissionDemo::GetCurrentEffectName() const noexcept {
	return kSubmissionPresets[presetIndex_].name;
}

bool PostEffectSubmissionDemo::IsDissolveActive() const noexcept {
	return kSubmissionPresets[presetIndex_].preset == PostEffectSystem::DemoPreset::Dissolve;
}

uint32_t PostEffectSubmissionDemo::GetDissolvePercent() const noexcept {
	return dissolvePercent_;
}

void PostEffectSubmissionDemo::UpdateWindowTitle(WinApp& winApp) const {
	std::string title =
		std::string("CG5 Farm PostEffect Demo - ") + GetCurrentEffectName();
	if (IsDissolveActive()) {
		title += " " + std::to_string(dissolvePercent_) + "%";
	}
	title += " [Space: Next]";
	SetWindowTextA(winApp.GetHwnd(), title.c_str());
}
