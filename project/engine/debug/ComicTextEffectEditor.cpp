#include "debug/ComicTextEffectEditor.h"

#ifdef USE_IMGUI

#include "externals/imgui/imgui.h"

#include <algorithm>
#include <cstring>

ComicTextEffectEditor::ComicTextEffectEditor() {
	strcpy_s(presetName_.data(), presetName_.size(), "HeavyImpact");
	strcpy_s(texturePath_.data(), texturePath_.size(), preset_.texturePath.c_str());
	strcpy_s(text_.data(), text_.size(), preset_.text.c_str());
}

void ComicTextEffectEditor::Draw(ComicTextEffectSystem& system, const Vector3& previewPosition) {
	ImGui::TextUnformatted("英字・数字・カタカナを自由入力して3D位置へ重ねます。");
	ImGui::InputText("Preset Name##Comic", presetName_.data(), presetName_.size());
	ImGui::Checkbox("Editable Bitmap Text##Comic", &preset_.useEditableText);
	ImGui::InputText("Display Text##Comic", text_.data(), text_.size());
	ImGui::SliderFloat("Text Scale##Comic", &preset_.textScale, 0.1f, 5.0f);
	ImGui::SliderFloat("Character Spacing##Comic", &preset_.characterSpacing, -96.0f, 100.0f);
	ImGui::DragFloat2("Red Extrusion Offset##Comic", &preset_.extrusionOffset.x, 0.5f, -100.0f, 100.0f);
	ImGui::ColorEdit4("Text Color##Comic", &preset_.textColor.x);
	ImGui::ColorEdit4("Extrusion Color##Comic", &preset_.extrusionColor.x);
	ImGui::SeparatorText("Image fallback");
	ImGui::InputText("Texture Path##Comic", texturePath_.data(), texturePath_.size());
	ImGui::DragFloat2("Display Size##Comic", &preset_.size.x, 2.0f, 8.0f, 1600.0f);
	ImGui::DragFloat2("Screen Offset##Comic", &preset_.screenOffset.x, 1.0f, -1000.0f, 1000.0f);
	ImGui::DragFloat2("Drift##Comic", &preset_.drift.x, 1.0f, -500.0f, 500.0f);
	ImGui::ColorEdit4("Tint##Comic", &preset_.color.x);
	ImGui::SliderFloat("Duration##Comic", &preset_.duration, 0.05f, 5.0f, "%.2f sec");
	ImGui::SliderFloat("Start Scale##Comic", &preset_.startScale, 0.0f, 3.0f);
	ImGui::SliderFloat("Peak Scale##Comic", &preset_.peakScale, 0.0f, 3.0f);
	ImGui::SliderFloat("End Scale##Comic", &preset_.endScale, 0.0f, 3.0f);
	ImGui::SliderFloat("Pop Timing##Comic", &preset_.popFraction, 0.01f, 0.95f);
	ImGui::SliderFloat("Fade Portion##Comic", &preset_.fadeFraction, 0.01f, 1.0f);
	ImGui::SliderAngle("Rotation##Comic", &preset_.rotation, -180.0f, 180.0f);
	ImGui::SliderFloat("Shake Amount##Comic", &preset_.shakeAmplitude, 0.0f, 100.0f);
	ImGui::SliderFloat("Shake Speed##Comic", &preset_.shakeFrequency, 0.0f, 200.0f);

	preset_.texturePath = texturePath_.data();
	preset_.text = text_.data();
	if (ImGui::Button("Preview Comic Text")) {
		status_ = system.Play(preset_, previewPosition) ? "Preview started." : "Preview failed.";
	}
	ImGui::SameLine();
	if (ImGui::Button("Save JSON##Comic")) {
		status_ = ComicTextEffectSystem::SavePreset(presetName_.data(), preset_) ?
			"Saved Settings/effects/comic JSON." : "Save failed. Use letters, numbers, _ or -.";
	}
	ImGui::SameLine();
	if (ImGui::Button("Load JSON##Comic")) {
		ComicTextEffectPreset loaded{};
		if (ComicTextEffectSystem::LoadPreset(presetName_.data(), loaded)) {
			preset_ = loaded;
			std::fill(texturePath_.begin(), texturePath_.end(), '\0');
			strcpy_s(texturePath_.data(), texturePath_.size(), preset_.texturePath.c_str());
			std::fill(text_.begin(), text_.end(), '\0');
			strcpy_s(text_.data(), text_.size(), preset_.text.c_str());
			status_ = "Loaded.";
		} else {
			status_ = "Load failed.";
		}
	}
	if (!status_.empty()) {
		ImGui::TextWrapped("%s", status_.c_str());
	}
	ImGui::SeparatorText("Runtime usage");
	ImGui::TextWrapped("comicTextEffects.Play(\"%s\", hitWorldPosition);", presetName_.data());
}

#endif
