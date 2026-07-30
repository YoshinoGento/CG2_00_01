#include "debug/PostEffectDebugWindow.h"

#include "base/ImGuiManager.h"

#include <algorithm>
#include <array>
#include <iterator>

namespace {
enum class DebugUiLanguage {
	English,
};

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
	"RandomNoise",
	"HSV Filter",
};

const char* Text(DebugUiLanguage, const char* english) noexcept {
	return english;
}

int ToEffectIndex(DirectXCommon::FullscreenPostEffectType effect) noexcept {
	return std::clamp(
		static_cast<int>(effect),
		0,
		static_cast<int>(kEffectNames.size()) - 1);
}

bool DrawNoiseTexture(
	PostEffectSystem::Settings& settings,
	const PostEffectSystem& system,
	DebugUiLanguage language) {
	const auto& noiseNames = system.GetNoiseNames();
	if (noiseNames.empty()) {
		ImGui::TextDisabled("%s", Text(language, "No noise textures are loaded."));
		return false;
	}

	settings.selectedNoiseIndex =
		(std::min)(settings.selectedNoiseIndex, noiseNames.size() - 1);
	bool changed = false;
	if (ImGui::BeginCombo(
		Text(language, "Noise Texture"),
		noiseNames[settings.selectedNoiseIndex].c_str())) {
		for (std::size_t index = 0; index < noiseNames.size(); ++index) {
			const bool selected = index == settings.selectedNoiseIndex;
			if (ImGui::Selectable(noiseNames[index].c_str(), selected)) {
				settings.selectedNoiseIndex = index;
				changed = true;
			}
			if (selected) {
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}
	return changed;
}

bool DrawEffectParameters(
	int effectIndex,
	PostEffectSystem::Settings& settings,
	const PostEffectSystem& system,
	DebugUiLanguage language) {
	bool changed = false;
	switch (static_cast<DirectXCommon::FullscreenPostEffectType>(effectIndex)) {
	case DirectXCommon::FullscreenPostEffectType::Copy:
		ImGui::TextDisabled("%s", Text(language, "This effect has no adjustable parameters."));
		break;
	case DirectXCommon::FullscreenPostEffectType::Grayscale:
		changed |= ImGui::SliderFloat(
			Text(language, "Grayscale Amount"),
			&settings.grayscaleIntensity,
			0.0f,
			1.0f,
			"%.2f");
		break;
	case DirectXCommon::FullscreenPostEffectType::Sepia:
		changed |= ImGui::SliderFloat(
			Text(language, "Sepia Amount"),
			&settings.sepiaIntensity,
			0.0f,
			1.0f,
			"%.2f");
		break;
	case DirectXCommon::FullscreenPostEffectType::Blur:
		changed |= ImGui::SliderFloat(
			Text(language, "Blur Strength"),
			&settings.blurStrength,
			0.0f,
			16.0f,
			"%.2f");
		break;
	case DirectXCommon::FullscreenPostEffectType::Bloom:
		changed |= ImGui::SliderFloat(
			Text(language, "Bloom Threshold"),
			&settings.bloomThreshold,
			0.0f,
			1.0f,
			"%.2f");
		changed |= ImGui::SliderFloat(
			Text(language, "Bloom Intensity"),
			&settings.bloomIntensity,
			0.0f,
			8.0f,
			"%.2f");
		changed |= ImGui::SliderFloat(
			Text(language, "Bloom Radius"),
			&settings.bloomRadius,
			0.0f,
			32.0f,
			"%.1f");
		changed |= ImGui::SliderFloat(
			Text(language, "Bloom Soft Knee"),
			&settings.bloomSoftKnee,
			0.001f,
			1.0f,
			"%.3f");
		break;
	case DirectXCommon::FullscreenPostEffectType::BoxFilter3x3:
	case DirectXCommon::FullscreenPostEffectType::BoxFilter5x5:
		ImGui::TextDisabled("%s", Text(language, "Kernel size is fixed by the selected effect."));
		break;
	case DirectXCommon::FullscreenPostEffectType::RadialBlur:
		changed |= ImGui::SliderFloat(
			Text(language, "Radial Center X"),
			&settings.radialBlurCenter.x,
			0.0f,
			1.0f,
			"%.2f");
		changed |= ImGui::SliderFloat(
			Text(language, "Radial Center Y"),
			&settings.radialBlurCenter.y,
			0.0f,
			1.0f,
			"%.2f");
		changed |= ImGui::SliderFloat(
			Text(language, "Radial Blur Width"),
			&settings.radialBlurWidth,
			0.0f,
			0.1f,
			"%.4f");
		changed |= ImGui::SliderFloat(
			Text(language, "Radial Blur Intensity"),
			&settings.radialBlurIntensity,
			0.0f,
			1.0f,
			"%.2f");
		changed |= ImGui::SliderInt(
			Text(language, "Radial Sample Count"),
			&settings.radialBlurSampleCount,
			1,
			32);
		break;
	case DirectXCommon::FullscreenPostEffectType::Dissolve:
		changed |= DrawNoiseTexture(settings, system, language);
		changed |= ImGui::SliderFloat(
			Text(language, "Dissolve Threshold"),
			&settings.dissolveThreshold,
			0.0f,
			1.0f,
			"%.2f");
		changed |= ImGui::SliderFloat(
			Text(language, "Dissolve Edge Width"),
			&settings.dissolveEdgeWidth,
			0.001f,
			0.2f,
			"%.3f");
		changed |= ImGui::SliderFloat(
			Text(language, "Dissolve Edge Intensity"),
			&settings.dissolveEdgeIntensity,
			0.0f,
			5.0f,
			"%.2f");
		changed |= ImGui::ColorEdit3(
			Text(language, "Dissolve Edge Color"),
			&settings.dissolveEdgeColor.x);
		changed |= ImGui::Checkbox(
			Text(language, "Dissolve Enable Edge"),
			&settings.dissolveEnableEdge);
		break;
	case DirectXCommon::FullscreenPostEffectType::OutlineLuminance:
		changed |= ImGui::SliderFloat(
			Text(language, "Outline Threshold"),
			&settings.outlineThreshold,
			0.0f,
			1.0f,
			"%.3f");
		changed |= ImGui::SliderFloat(
			Text(language, "Outline Intensity"),
			&settings.outlineIntensity,
			0.0f,
			8.0f,
			"%.2f");
		changed |= ImGui::SliderFloat(
			Text(language, "Outline Thickness"),
			&settings.outlineThickness,
			0.0f,
			8.0f,
			"%.1f");
		break;
	case DirectXCommon::FullscreenPostEffectType::OutlineDepth:
		changed |= ImGui::SliderFloat(
			Text(language, "Depth Threshold"),
			&settings.depthOutlineThreshold,
			0.0001f,
			0.1f,
			"%.5f");
		changed |= ImGui::SliderFloat(
			Text(language, "Outline Intensity"),
			&settings.depthOutlineIntensity,
			0.0f,
			8.0f,
			"%.2f");
		changed |= ImGui::SliderFloat(
			Text(language, "Outline Thickness"),
			&settings.depthOutlineThickness,
			1.0f,
			8.0f,
			"%.1f");
		changed |= ImGui::Checkbox(
			Text(language, "Linearize Depth"),
			&settings.depthOutlineLinearize);
		break;
	case DirectXCommon::FullscreenPostEffectType::OutlineNormal:
		changed |= ImGui::SliderFloat(
			Text(language, "Normal Outline Threshold"),
			&settings.normalOutlineThreshold,
			0.0f,
			4.0f,
			"%.2f");
		changed |= ImGui::SliderFloat(
			Text(language, "Outline Intensity"),
			&settings.normalOutlineIntensity,
			0.0f,
			8.0f,
			"%.2f");
		changed |= ImGui::SliderFloat(
			Text(language, "Outline Thickness"),
			&settings.normalOutlineThickness,
			1.0f,
			8.0f,
			"%.1f");
		break;
	case DirectXCommon::FullscreenPostEffectType::OutlineDepthNormal:
		changed |= ImGui::SliderFloat(
			Text(language, "Depth Threshold"),
			&settings.depthOutlineThreshold,
			0.0001f,
			0.1f,
			"%.5f");
		changed |= ImGui::SliderFloat(
			Text(language, "Depth Outline Intensity"),
			&settings.depthOutlineIntensity,
			0.0f,
			8.0f,
			"%.2f");
		changed |= ImGui::SliderFloat(
			Text(language, "Depth Outline Thickness"),
			&settings.depthOutlineThickness,
			1.0f,
			8.0f,
			"%.1f");
		changed |= ImGui::Checkbox(
			Text(language, "Linearize Depth"),
			&settings.depthOutlineLinearize);
		changed |= ImGui::SliderFloat(
			Text(language, "Normal Outline Threshold"),
			&settings.normalOutlineThreshold,
			0.0f,
			4.0f,
			"%.2f");
		changed |= ImGui::SliderFloat(
			Text(language, "Normal Outline Intensity"),
			&settings.normalOutlineIntensity,
			0.0f,
			8.0f,
			"%.2f");
		changed |= ImGui::SliderFloat(
			Text(language, "Normal Outline Thickness"),
			&settings.normalOutlineThickness,
			1.0f,
			8.0f,
			"%.1f");
		break;
	case DirectXCommon::FullscreenPostEffectType::Vignette:
		changed |= ImGui::SliderFloat(
			Text(language, "Vignette Scale"),
			&settings.vignetteScale,
			0.0f,
			64.0f,
			"%.1f");
		changed |= ImGui::SliderFloat(
			Text(language, "Vignette Power"),
			&settings.vignettePower,
			0.01f,
			8.0f,
			"%.2f");
		changed |= ImGui::SliderFloat(
			Text(language, "Vignette Intensity"),
			&settings.vignetteIntensity,
			0.0f,
			1.0f,
			"%.2f");
		break;
	case DirectXCommon::FullscreenPostEffectType::RandomNoise: {
		constexpr const char* kRandomNoiseModes[] = { "WhiteNoiseOnly", "MultiplyScene" };
		changed |= ImGui::Combo(
			Text(language, "Random Noise Mode"),
			&settings.randomNoiseMode,
			kRandomNoiseModes,
			static_cast<int>(std::size(kRandomNoiseModes)));
		changed |= ImGui::SliderFloat(
			Text(language, "Random Noise Strength"),
			&settings.randomNoiseStrength,
			0.0f,
			1.0f,
			"%.2f");
		changed |= ImGui::SliderFloat(
			Text(language, "Random Noise Scale"),
			&settings.randomNoiseScale,
			1.0f,
			2000.0f,
			"%.0f");
		changed |= ImGui::Checkbox(
			Text(language, "Random Noise Animate"),
			&settings.randomNoiseAnimate);
		changed |= ImGui::SliderFloat(
			Text(language, "Random Noise Time Speed"),
			&settings.randomNoiseTimeSpeed,
			0.0f,
			10.0f,
			"%.2f");
		break;
	}
	case DirectXCommon::FullscreenPostEffectType::HSVFilter:
		changed |= ImGui::SliderFloat(
			Text(language, "HSV Hue"),
			&settings.hsvHue,
			-1.0f,
			1.0f,
			"%.2f");
		changed |= ImGui::SliderFloat(
			Text(language, "HSV Saturation"),
			&settings.hsvSaturation,
			-1.0f,
			1.0f,
			"%.2f");
		changed |= ImGui::SliderFloat(
			Text(language, "HSV Value"),
			&settings.hsvValue,
			-1.0f,
			1.0f,
			"%.2f");
		if (ImGui::Button(Text(language, "Reset HSV"))) {
			settings.hsvHue = 0.0f;
			settings.hsvSaturation = 0.0f;
			settings.hsvValue = 0.0f;
			changed = true;
		}
		break;
	case DirectXCommon::FullscreenPostEffectType::LinearToSRGB:
	case DirectXCommon::FullscreenPostEffectType::Count:
	default:
		break;
	}
	return changed;
}
} // namespace

PostEffectDebugActions PostEffectDebugWindow::Draw(const PostEffectSystem& system) {
	constexpr DebugUiLanguage language = DebugUiLanguage::English;
	PostEffectDebugActions actions;
	PostEffectSystem::Settings editedSettings = system.GetSettings();
	bool settingsChanged = false;

	if (!ImGui::Begin(Text(language, "PostEffect Workspace###Fullscreen PostEffect"))) {
		ImGui::End();
		return actions;
	}

	ImGui::Spacing();
	const bool chainModeEnabled = system.IsChainModeEnabled();
	const int activeEffectIndex = ToEffectIndex(editedSettings.effect);
	ImGui::TextUnformatted(Text(language, "Pipeline Status"));
	ImGui::SameLine();
	ImGui::TextColored(
		ImVec4(0.35f, 0.90f, 0.45f, 1.0f),
		"%s",
		Text(language, "READY"));
	ImGui::Text(
		"%s: %s",
		Text(language, "Mode"),
		Text(language, chainModeEnabled ? "Chain" : "Single"));
	ImGui::Text(
		"%s: %s",
		Text(language, "Active Effect"),
		chainModeEnabled ? Text(language, "Multiple Passes") : kEffectNames[activeEffectIndex]);
	ImGui::TextUnformatted("GammaCorrection: LinearToSRGB (Fixed Last)");
	ImGui::TextUnformatted("Output: FinalDisplayTexture");

	ImGui::SeparatorText(Text(language, "Effect Mode"));
	bool requestedChainMode = chainModeEnabled;
	if (ImGui::Checkbox(Text(language, "PostEffect Chain Mode"), &requestedChainMode)) {
		actions.chainModeEnabled = requestedChainMode;
	}

	if (!chainModeEnabled) {
		int selectedEffect = activeEffectIndex;
		if (ImGui::Combo(
			Text(language, "Fullscreen Effect"),
			&selectedEffect,
			kEffectNames.data(),
			static_cast<int>(kEffectNames.size()))) {
			editedSettings.effect =
				static_cast<DirectXCommon::FullscreenPostEffectType>(selectedEffect);
			parameterTarget_ = selectedEffect;
			settingsChanged = true;
		}
		parameterTarget_ = ToEffectIndex(editedSettings.effect);
	} else {
		ImGui::Text(
			"%s: %zu / %zu",
			Text(language, "Enabled Passes"),
			system.GetEnabledChainPassCount(),
			system.GetChainPassCount());
		if (ImGui::BeginTable(
			"##PostEffectChainPasses",
			2,
			ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_BordersInnerV)) {
			for (std::size_t index = 0; index < system.GetChainPassCount(); ++index) {
				ImGui::TableNextColumn();
				ImGui::PushID(static_cast<int>(index));
				bool enabled = system.IsChainPassEnabled(index);
				if (ImGui::Checkbox(system.GetChainPassName(index), &enabled)) {
					actions.chainPassChange = PostEffectChainPassChange{ index, enabled };
				}
				ImGui::PopID();
			}
			ImGui::EndTable();
		}

		parameterTarget_ = std::clamp(
			parameterTarget_,
			1,
			static_cast<int>(kEffectNames.size()) - 1);
		if (ImGui::BeginCombo(
			Text(language, "Parameter Target"),
			kEffectNames[parameterTarget_])) {
			for (int index = 1; index < static_cast<int>(kEffectNames.size()); ++index) {
				const bool selected = index == parameterTarget_;
				if (ImGui::Selectable(kEffectNames[index], selected)) {
					parameterTarget_ = index;
				}
				if (selected) {
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
	}

	ImGui::SeparatorText(Text(language, "Selected Effect Parameters"));
	ImGui::TextColored(
		ImVec4(0.45f, 0.78f, 1.0f, 1.0f),
		"%s",
		kEffectNames[parameterTarget_]);
	settingsChanged |=
		DrawEffectParameters(parameterTarget_, editedSettings, system, language);

	if (ImGui::CollapsingHeader(Text(language, "Pipeline Details"))) {
		ImGui::Text(
			"%s: %s",
			Text(language, "Chain Mode"),
			chainModeEnabled ? "ON" : "OFF");
		ImGui::Text(
			"%s: %zu / %zu",
			Text(language, "Enabled Passes"),
			system.GetEnabledChainPassCount(),
			system.GetChainPassCount());
		ImGui::TextUnformatted("Copy: Fallback when no chain pass is enabled");
		ImGui::TextUnformatted("GammaCorrection: Fixed Last");
		ImGui::TextUnformatted("Viewport/Swapchain: FinalDisplayTexture");
	}

	ImGui::Separator();
	if (ImGui::Button(Text(language, "Reset All PostEffect Parameters"), { -1.0f, 0.0f })) {
		actions.resetSettings = true;
	}

	if (settingsChanged) {
		actions.settings = editedSettings;
	}
	ImGui::End();
	return actions;
}
