#include "editor/CG5DemoWindow.h"

#ifdef USE_IMGUI
#include "base/ImGuiManager.h"
#endif

CG5DemoActions CG5DemoWindow::Draw(
	const CG5DemoViewData& viewData,
	EditorLanguage language) {
	CG5DemoActions actions;
#ifdef USE_IMGUI
	if (!open_) {
		return actions;
	}

	const auto text = [language](const char* english) {
		return editor::Localize(language, english);
	};
	ImGui::SetNextWindowSize({ 390.0f, 470.0f }, ImGuiCond_FirstUseEver);
	if (!ImGui::Begin(text("CG5 Demo###CG5Demo"), &open_)) {
		ImGui::End();
		return actions;
	}

	ImGui::TextUnformatted(text("Required Effect"));
	ImGui::SameLine();
	const bool requirementReady =
		!viewData.chainModeEnabled &&
		viewData.effect == CG5DemoEffectState::Grayscale &&
		viewData.grayscaleIntensity > 0.0f;
	ImGui::TextColored(
		requirementReady
			? ImVec4(0.35f, 0.90f, 0.45f, 1.0f)
			: ImVec4(1.0f, 0.62f, 0.20f, 1.0f),
		"%s",
		text(requirementReady ? "READY" : "NOT ACTIVE"));

	ImGui::TextWrapped("%s", text("Compare the same Farm scene before and after Grayscale."));
	ImGui::Spacing();
	const float buttonWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
	if (ImGui::Button(text("Original Color"), { buttonWidth, 34.0f })) {
		actions.preset = CG5DemoPresetAction::Original;
	}
	ImGui::SameLine();
	if (ImGui::Button(text("Required Grayscale"), { buttonWidth, 34.0f })) {
		actions.preset = CG5DemoPresetAction::RequiredGrayscale;
	}

	float intensity = viewData.grayscaleIntensity;
	if (ImGui::SliderFloat(text("Grayscale Amount"), &intensity, 0.0f, 1.0f, "%.2f")) {
		actions.grayscaleIntensity = intensity;
	}

	ImGui::Separator();
	ImGui::TextUnformatted(text("Bonus Effect"));
	ImGui::SameLine();
	const bool bonusReady =
		!viewData.chainModeEnabled &&
		viewData.effect == CG5DemoEffectState::DepthBasedOutline;
	ImGui::TextColored(
		bonusReady
			? ImVec4(0.35f, 0.90f, 0.45f, 1.0f)
			: ImVec4(0.65f, 0.72f, 0.90f, 1.0f),
		"%s",
		text(bonusReady ? "READY" : "NOT ACTIVE"));
	ImGui::TextWrapped("%s", text("Highlight Farm depth boundaries for the scored bonus demo."));
	if (ImGui::Button(text("Depth-Based Outline"), { -1.0f, 34.0f })) {
		actions.preset = CG5DemoPresetAction::DepthBasedOutline;
	}

	CG5DepthOutlineParameters outline = viewData.depthOutline;
	bool outlineChanged = false;
	outlineChanged |= ImGui::SliderFloat(
		text("Depth Threshold"), &outline.threshold, 0.0001f, 0.02f, "%.5f");
	outlineChanged |= ImGui::SliderFloat(
		text("Outline Intensity"), &outline.intensity, 0.0f, 8.0f, "%.2f");
	outlineChanged |= ImGui::SliderFloat(
		text("Outline Thickness"), &outline.thickness, 1.0f, 8.0f, "%.1f");
	outlineChanged |= ImGui::Checkbox(text("Linearize Depth"), &outline.linearize);
	if (outlineChanged) {
		actions.depthOutline = outline;
	}

	const char* currentEffect = "Other";
	switch (viewData.effect) {
	case CG5DemoEffectState::Original:
		currentEffect = "Copy";
		break;
	case CG5DemoEffectState::Grayscale:
		currentEffect = "Grayscale";
		break;
	case CG5DemoEffectState::DepthBasedOutline:
		currentEffect = "DepthBasedOutline";
		break;
	case CG5DemoEffectState::Other:
	default:
		break;
	}

	ImGui::Separator();
	ImGui::Text("%s: %s", text("Current Effect"), currentEffect);
	ImGui::Text("ChainMode: %s", viewData.chainModeEnabled ? "ON" : "OFF");
	ImGui::TextUnformatted("GammaCorrection: LinearToSRGB (Fixed Last)");
	ImGui::TextUnformatted("Output: FinalDisplayTexture");
	ImGui::End();
#else
	(void)viewData;
	(void)language;
#endif
	return actions;
}
