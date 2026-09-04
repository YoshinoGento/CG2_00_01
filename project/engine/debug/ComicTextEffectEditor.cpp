#include "debug/ComicTextEffectEditor.h"

#ifdef USE_IMGUI

#include "externals/imgui/imgui.h"

#include <algorithm>
#include <cfloat>
#include <cstring>
#include <filesystem>

namespace {
constexpr const char* kComicPresetDirectory = "Settings/effects/comic/";
}

ComicTextEffectEditor::ComicTextEffectEditor() {
	strcpy_s(presetName_.data(), presetName_.size(), "HeavyImpact");
	strcpy_s(texturePath_.data(), texturePath_.size(), preset_.texturePath.c_str());
	strcpy_s(text_.data(), text_.size(), preset_.text.c_str());
	RefreshPresetList();
}

void ComicTextEffectEditor::RefreshPresetList() {
	presetNames_.clear();
	std::error_code error;
	if (std::filesystem::exists(kComicPresetDirectory, error)) {
		for (const std::filesystem::directory_entry& entry :
			std::filesystem::directory_iterator(kComicPresetDirectory, error)) {
			if (error) break;
			if (entry.is_regular_file(error) && entry.path().extension() == ".json") {
				presetNames_.push_back(entry.path().stem().string());
			}
		}
	}
	std::sort(presetNames_.begin(), presetNames_.end());
	selectedPresetIndex_ = -1;
	for (int index = 0; index < static_cast<int>(presetNames_.size()); ++index) {
		if (presetNames_[index] == presetName_.data()) {
			selectedPresetIndex_ = index;
			break;
		}
	}
}

bool ComicTextEffectEditor::LoadCurrentPreset() {
	ComicTextEffectPreset loaded{};
	if (!ComicTextEffectSystem::LoadPreset(presetName_.data(), loaded)) {
		status_ = "Load failed.";
		return false;
	}
	preset_ = loaded;
	strncpy_s(texturePath_.data(), texturePath_.size(), preset_.texturePath.c_str(), _TRUNCATE);
	strncpy_s(text_.data(), text_.size(), preset_.text.c_str(), _TRUNCATE);
	status_ = "Loaded.";
	return true;
}

void ComicTextEffectEditor::SelectPreset(int index) {
	if (index < 0 || index >= static_cast<int>(presetNames_.size())) return;
	selectedPresetIndex_ = index;
	strncpy_s(presetName_.data(), presetName_.size(), presetNames_[index].c_str(), _TRUNCATE);
	LoadCurrentPreset();
}

bool ComicTextEffectEditor::SaveCurrentPreset(bool overwrite) {
	const std::filesystem::path path = std::filesystem::path(kComicPresetDirectory) /
		(std::string(presetName_.data()) + ".json");
	if (!overwrite && std::filesystem::exists(path)) {
		status_ = "Save failed: preset already exists. Use Overwrite.";
		return false;
	}
	preset_.texturePath = texturePath_.data();
	preset_.text = text_.data();
	if (!ComicTextEffectSystem::SavePreset(presetName_.data(), preset_)) {
		status_ = "Save failed. Use letters, numbers, _ or -.";
		return false;
	}
	status_ = overwrite ? "Overwritten." : "Saved.";
	RefreshPresetList();
	return true;
}

void ComicTextEffectEditor::DeleteSelectedPreset() {
	if (selectedPresetIndex_ < 0 || selectedPresetIndex_ >= static_cast<int>(presetNames_.size())) {
		status_ = "Delete failed: select a preset first.";
		return;
	}
	const std::filesystem::path path = std::filesystem::path(kComicPresetDirectory) /
		(presetNames_[selectedPresetIndex_] + ".json");
	std::error_code error;
	if (!std::filesystem::remove(path, error) || error) {
		status_ = "Delete failed: " + path.string();
		return;
	}
	status_ = "Deleted: " + path.string();
	RefreshPresetList();
}

void ComicTextEffectEditor::Draw(ComicTextEffectSystem& system, const Vector3& previewPosition) {
	ImGui::TextUnformatted("英字・数字・カタカナを自由入力して3D位置へ重ねます。");
	ImGui::SeparatorText("Preset Library");
	if (ImGui::Button("Refresh List##Comic")) RefreshPresetList();
	ImGui::SameLine();
	ImGui::TextDisabled("%d preset(s)", static_cast<int>(presetNames_.size()));
	if (ImGui::BeginListBox("Saved Presets##Comic", ImVec2(-FLT_MIN, 110.0f))) {
		for (int index = 0; index < static_cast<int>(presetNames_.size()); ++index) {
			const bool selected = index == selectedPresetIndex_;
			if (ImGui::Selectable(presetNames_[index].c_str(), selected)) SelectPreset(index);
			if (selected) ImGui::SetItemDefaultFocus();
		}
		ImGui::EndListBox();
	}
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
	if (ImGui::Button("Save New##Comic")) {
		SaveCurrentPreset(false);
	}
	ImGui::SameLine();
	if (ImGui::Button("Overwrite##Comic")) {
		SaveCurrentPreset(true);
	}
	ImGui::SameLine();
	if (ImGui::Button("Load##Comic")) {
		LoadCurrentPreset();
	}
	ImGui::SameLine();
	if (ImGui::Button("Delete##Comic")) {
		if (selectedPresetIndex_ >= 0 && selectedPresetIndex_ < static_cast<int>(presetNames_.size())) {
			pendingDeletePreset_ = presetNames_[selectedPresetIndex_];
			ImGui::OpenPopup("Delete Comic Preset?");
		} else {
			status_ = "Delete failed: select a preset first.";
		}
	}
	if (ImGui::BeginPopupModal("Delete Comic Preset?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::Text("Delete '%s'?", pendingDeletePreset_.c_str());
		ImGui::TextUnformatted("This cannot be undone.");
		if (ImGui::Button("Delete##ConfirmComic", ImVec2(120.0f, 0.0f))) {
			DeleteSelectedPreset();
			pendingDeletePreset_.clear();
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel##Comic", ImVec2(120.0f, 0.0f))) {
			pendingDeletePreset_.clear();
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
	if (!status_.empty()) {
		ImGui::TextWrapped("%s", status_.c_str());
	}
	ImGui::SeparatorText("Runtime usage");
	ImGui::TextWrapped("comicTextEffects.Play(\"%s\", hitWorldPosition);", presetName_.data());
}

#endif
