#include "debug/PostEffectDebugWindow.h"

#ifdef USE_IMGUI
#include "base/ImGuiManager.h"

#include <algorithm>
#include <array>
#include <iterator>

namespace {
constexpr std::array kEffectNames = {
	"None / Copy",
	"Grayscale",
	"Sepia",
	"Blur",
	"Bloom",
	"BoxFilter 3x3",
	"BoxFilter 5x5",
	"Radial Blur",
	"Dissolve",
	"Outline Luminance",
	"Outline Depth",
	"Outline Normal",
	"Outline Depth + Normal",
	"Vignette",
	"Random Noise",
	"HSV Filter",
	"Linear to sRGB",
};

int ToEffectIndex(DirectXCommon::FullscreenPostEffectType effect) noexcept {
	return std::clamp(
		static_cast<int>(effect),
		0,
		static_cast<int>(kEffectNames.size()) - 1);
}

bool DrawEffectParameters(PostEffectSystem::Settings& settings) {
	bool changed = false;
	switch (settings.effect) {
	case DirectXCommon::FullscreenPostEffectType::Grayscale:
		changed |= ImGui::SliderFloat("Grayscale Amount", &settings.grayscaleIntensity, 0.0f, 1.0f);
		break;
	case DirectXCommon::FullscreenPostEffectType::Sepia:
		changed |= ImGui::SliderFloat("Sepia Amount", &settings.sepiaIntensity, 0.0f, 1.0f);
		break;
	case DirectXCommon::FullscreenPostEffectType::Blur:
		changed |= ImGui::SliderFloat("Blur Strength", &settings.blurStrength, 0.0f, 16.0f);
		break;
	case DirectXCommon::FullscreenPostEffectType::Bloom:
		changed |= ImGui::SliderFloat("Bloom Threshold", &settings.bloomThreshold, 0.0f, 1.0f);
		changed |= ImGui::SliderFloat("Bloom Intensity", &settings.bloomIntensity, 0.0f, 8.0f);
		changed |= ImGui::SliderFloat("Bloom Radius", &settings.bloomRadius, 0.0f, 32.0f);
		break;
	case DirectXCommon::FullscreenPostEffectType::RadialBlur:
		changed |= ImGui::SliderFloat2("Radial Center", &settings.radialBlurCenter.x, 0.0f, 1.0f);
		changed |= ImGui::SliderFloat("Radial Width", &settings.radialBlurWidth, 0.0f, 0.1f, "%.4f");
		changed |= ImGui::SliderFloat("Radial Intensity", &settings.radialBlurIntensity, 0.0f, 1.0f);
		changed |= ImGui::SliderInt("Radial Samples", &settings.radialBlurSampleCount, 1, 32);
		break;
	case DirectXCommon::FullscreenPostEffectType::Dissolve:
		changed |= ImGui::SliderFloat("Dissolve Threshold", &settings.dissolveThreshold, 0.0f, 1.0f);
		changed |= ImGui::SliderFloat("Dissolve Edge Width", &settings.dissolveEdgeWidth, 0.001f, 0.2f);
		break;
	case DirectXCommon::FullscreenPostEffectType::OutlineLuminance:
		changed |= ImGui::SliderFloat("Outline Threshold", &settings.outlineThreshold, 0.0f, 1.0f);
		changed |= ImGui::SliderFloat("Outline Intensity", &settings.outlineIntensity, 0.0f, 8.0f);
		break;
	case DirectXCommon::FullscreenPostEffectType::OutlineDepth:
	case DirectXCommon::FullscreenPostEffectType::OutlineDepthNormal:
		changed |= ImGui::SliderFloat("Depth Threshold", &settings.depthOutlineThreshold, 0.0001f, 0.1f, "%.5f");
		changed |= ImGui::SliderFloat("Depth Intensity", &settings.depthOutlineIntensity, 0.0f, 8.0f);
		break;
	case DirectXCommon::FullscreenPostEffectType::OutlineNormal:
		changed |= ImGui::SliderFloat("Normal Threshold", &settings.normalOutlineThreshold, 0.0f, 4.0f);
		changed |= ImGui::SliderFloat("Normal Intensity", &settings.normalOutlineIntensity, 0.0f, 8.0f);
		break;
	case DirectXCommon::FullscreenPostEffectType::Vignette:
		changed |= ImGui::SliderFloat("Vignette Scale", &settings.vignetteScale, 0.0f, 64.0f);
		changed |= ImGui::SliderFloat("Vignette Power", &settings.vignettePower, 0.01f, 8.0f);
		changed |= ImGui::SliderFloat("Vignette Intensity", &settings.vignetteIntensity, 0.0f, 1.0f);
		break;
	case DirectXCommon::FullscreenPostEffectType::RandomNoise:
		changed |= ImGui::SliderFloat("Noise Strength", &settings.randomNoiseStrength, 0.0f, 1.0f);
		changed |= ImGui::SliderFloat("Noise Scale", &settings.randomNoiseScale, 1.0f, 2000.0f);
		changed |= ImGui::Checkbox("Animate Noise", &settings.randomNoiseAnimate);
		break;
	case DirectXCommon::FullscreenPostEffectType::HSVFilter:
		changed |= ImGui::SliderFloat("Hue", &settings.hsvHue, -1.0f, 1.0f);
		changed |= ImGui::SliderFloat("Saturation", &settings.hsvSaturation, -1.0f, 1.0f);
		changed |= ImGui::SliderFloat("Value", &settings.hsvValue, -1.0f, 1.0f);
		break;
	default:
		ImGui::TextDisabled("No adjustable parameters.");
		break;
	}
	return changed;
}
} // namespace
#endif

PostEffectDebugActions PostEffectDebugWindow::Draw(
	const PostEffectSystem& system) {
	PostEffectDebugActions actions;
#ifdef USE_IMGUI
	PostEffectSystem::Settings editedSettings = system.GetSettings();
	bool settingsChanged = false;

	if (!ImGui::Begin("PostEffect Debug")) {
		ImGui::End();
		return actions;
	}

	int effectIndex = ToEffectIndex(editedSettings.effect);
	if (ImGui::Combo(
		"Fullscreen Effect",
		&effectIndex,
		kEffectNames.data(),
		static_cast<int>(kEffectNames.size()))) {
		editedSettings.effect = static_cast<DirectXCommon::FullscreenPostEffectType>(effectIndex);
		settingsChanged = true;
	}

	bool chainModeEnabled = system.IsChainModeEnabled();
	if (ImGui::Checkbox("PostEffect Chain Mode", &chainModeEnabled)) {
		actions.chainModeEnabled = chainModeEnabled;
	}
	if (chainModeEnabled && ImGui::CollapsingHeader("Chain Passes", ImGuiTreeNodeFlags_DefaultOpen)) {
		for (std::size_t index = 0; index < system.GetChainPassCount(); ++index) {
			bool enabled = system.IsChainPassEnabled(index);
			ImGui::PushID(static_cast<int>(index));
			if (ImGui::Checkbox(system.GetChainPassName(index), &enabled)) {
				actions.chainPassChange = PostEffectChainPassChange{ index, enabled };
			}
			ImGui::PopID();
		}
	}

	ImGui::SeparatorText("Parameters");
	settingsChanged |= DrawEffectParameters(editedSettings);
	if (ImGui::Button("Reset Parameters")) {
		actions.resetSettings = true;
	}
	if (settingsChanged) {
		actions.settings = editedSettings;
	}
	ImGui::End();
#else
	(void)system;
#endif
	return actions;
}
