#pragma once

#include "editor/EditorLocalization.h"

#include <optional>

enum class CG5DemoPresetAction {
	Original,
	RequiredGrayscale,
	DepthBasedOutline,
};

enum class CG5DemoEffectState {
	Original,
	Grayscale,
	DepthBasedOutline,
	Other,
};

struct CG5DepthOutlineParameters {
	float threshold = 0.001f;
	float intensity = 1.0f;
	float thickness = 1.0f;
	bool linearize = true;
};

struct CG5DemoViewData {
	bool chainModeEnabled = false;
	CG5DemoEffectState effect = CG5DemoEffectState::Original;
	float grayscaleIntensity = 1.0f;
	CG5DepthOutlineParameters depthOutline;
};

struct CG5DemoActions {
	std::optional<CG5DemoPresetAction> preset;
	std::optional<float> grayscaleIntensity;
	std::optional<CG5DepthOutlineParameters> depthOutline;
};

// Compact CG5 comparison UI. PostEffect mutation remains in PostEffectSystem.
class CG5DemoWindow final {
public:
	[[nodiscard]] CG5DemoActions Draw(
		const CG5DemoViewData& viewData,
		EditorLanguage language);

	void SetOpen(bool open) noexcept { open_ = open; }
	[[nodiscard]] bool IsOpen() const noexcept { return open_; }

private:
	bool open_ = true;
};
