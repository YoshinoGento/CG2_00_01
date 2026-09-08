#include "debug/ComicTextEffectEditor.h"

#ifdef USE_IMGUI

#include "externals/imgui/imgui.h"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cstring>
#include <filesystem>

namespace {
constexpr const char* kComicPresetDirectory = "Settings/effects/comic/";

bool IsSafePresetName(const char* name) {
	if (!name || name[0] == '\0') return false;
	const std::string value(name);
	if (value == "." || value == ".." || value.back() == '.' || value.back() == ' ') return false;
	for (const unsigned char ch : value) {
		if (ch < 0x20 || ch == '<' || ch == '>' || ch == ':' || ch == '"' ||
			ch == '/' || ch == '\\' || ch == '|' || ch == '?' || ch == '*') return false;
	}
	return true;
}

std::string PathComponentUtf8(const std::filesystem::path& path) {
	const std::u8string value = path.u8string();
	return { reinterpret_cast<const char*>(value.data()), value.size() };
}

std::filesystem::path Utf8Path(const std::string& value) {
	return std::filesystem::path(std::u8string(
		reinterpret_cast<const char8_t*>(value.data()), value.size()));
}
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
				const std::string name = PathComponentUtf8(entry.path().stem());
				if (IsSafePresetName(name.c_str())) {
					presetNames_.push_back(name);
				}
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
	if (!IsSafePresetName(presetName_.data())) {
		status_ = "Save failed. The name is empty or contains a Windows-invalid character.";
		return false;
	}
	const std::filesystem::path path = Utf8Path(kComicPresetDirectory) /
		Utf8Path(std::string(presetName_.data()) + ".json");
	std::error_code existsError;
	const bool presetExists = std::filesystem::exists(path, existsError);
	if (existsError) {
		status_ = "Save failed: could not inspect preset path.";
		return false;
	}
	if (!overwrite && presetExists) {
		status_ = "Save failed: preset already exists. Use Overwrite.";
		return false;
	}
	if (overwrite && !presetExists) {
		status_ = "Overwrite failed: preset does not exist. Use Save New.";
		return false;
	}
	preset_.texturePath = texturePath_.data();
	preset_.text = text_.data();
	if (!ComicTextEffectSystem::SavePreset(presetName_.data(), preset_)) {
		status_ = "Save failed. The name is empty or contains a Windows-invalid character.";
		return false;
	}
	status_ = overwrite ? "Overwritten." : "Saved.";
	RefreshPresetList();
	return true;
}

void ComicTextEffectEditor::RenameSelectedPreset() {
	if (selectedPresetIndex_ < 0 || selectedPresetIndex_ >= static_cast<int>(presetNames_.size())) {
		status_ = "Rename failed: select a preset first.";
		return;
	}
	if (!IsSafePresetName(presetName_.data())) {
		status_ = "Rename failed. The name is empty or contains a Windows-invalid character.";
		return;
	}
	const std::string oldName = presetNames_[selectedPresetIndex_];
	const std::string newName = presetName_.data();
	if (oldName == newName) {
		status_ = "Rename skipped: the name has not changed.";
		return;
	}
	const std::filesystem::path newPath = Utf8Path(kComicPresetDirectory) / Utf8Path(newName + ".json");
	std::error_code error;
	if (std::filesystem::exists(newPath, error) || error) {
		status_ = "Rename failed: destination already exists or cannot be checked.";
		return;
	}
	if (!SaveCurrentPreset(false)) return;
	const std::filesystem::path oldPath = Utf8Path(kComicPresetDirectory) / Utf8Path(oldName + ".json");
	if (!std::filesystem::remove(oldPath, error) || error) {
		status_ = "Saved new name, but failed to remove old preset: " + oldName;
		return;
	}
	status_ = "Renamed: " + oldName + " -> " + newName;
	RefreshPresetList();
}

void ComicTextEffectEditor::DrawPresetLibrary() {
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
	if (selectedPresetIndex_ >= 0 && selectedPresetIndex_ < static_cast<int>(presetNames_.size())) {
		ImGui::TextDisabled("Selected: %s", presetNames_[selectedPresetIndex_].c_str());
	}
	ImGui::SetNextItemWidth(-FLT_MIN);
	ImGui::InputText("Preset Name##Comic", presetName_.data(), presetName_.size());
	ImGui::TextDisabled("Japanese is OK. Windows-invalid filename characters cannot be used.");
	if (ImGui::Button("Save New##Comic", ImVec2(120.0f, 0.0f))) SaveCurrentPreset(false);
	ImGui::SameLine();
	if (ImGui::Button("Overwrite##Comic", ImVec2(120.0f, 0.0f))) {
		const bool renamed = selectedPresetIndex_ >= 0 &&
			selectedPresetIndex_ < static_cast<int>(presetNames_.size()) &&
			presetNames_[selectedPresetIndex_] != presetName_.data();
		if (renamed) RenameSelectedPreset();
		else SaveCurrentPreset(true);
	}
	ImGui::SameLine();
	if (ImGui::Button("Load##Comic", ImVec2(90.0f, 0.0f))) LoadCurrentPreset();
	if (ImGui::Button("Rename Selected##Comic", ImVec2(170.0f, 0.0f))) RenameSelectedPreset();
	ImGui::SameLine();
	if (ImGui::Button("Delete Selected##Comic", ImVec2(170.0f, 0.0f))) {
		if (selectedPresetIndex_ >= 0 && selectedPresetIndex_ < static_cast<int>(presetNames_.size())) {
			pendingDeletePreset_ = presetNames_[selectedPresetIndex_];
			ImGui::OpenPopup("Delete Comic Preset?");
		} else status_ = "Delete failed: select a preset first.";
	}
	if (!status_.empty()) {
		ImGui::TextWrapped("Result: %s", status_.c_str());
	}
	if (ImGui::BeginPopupModal("Delete Comic Preset?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::Text("Delete '%s'?", pendingDeletePreset_.c_str());
		ImGui::TextUnformatted("This cannot be undone.");
		if (ImGui::Button("Delete##ConfirmComic", ImVec2(120.0f, 0.0f))) {
			DeleteSelectedPreset(); pendingDeletePreset_.clear(); ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel##Comic", ImVec2(120.0f, 0.0f))) {
			pendingDeletePreset_.clear(); ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
}

void ComicTextEffectEditor::DeleteSelectedPreset() {
	if (selectedPresetIndex_ < 0 || selectedPresetIndex_ >= static_cast<int>(presetNames_.size())) {
		status_ = "Delete failed: select a preset first.";
		return;
	}
	const std::filesystem::path path = Utf8Path(kComicPresetDirectory) /
		Utf8Path(presetNames_[selectedPresetIndex_] + ".json");
	std::error_code error;
	if (!std::filesystem::remove(path, error) || error) {
		status_ = "Delete failed: " + PathComponentUtf8(path);
		return;
	}
	status_ = "Deleted: " + PathComponentUtf8(path);
	RefreshPresetList();
}

void ComicTextEffectEditor::Draw(ComicTextEffectSystem& system, const Vector3& previewPosition,
	bool previewRequested) {
	ImGui::TextUnformatted("英字・数字・カタカナを自由入力して3D位置へ重ねます。");
	DrawPresetLibrary();
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
	if (ImGui::Button("Preview Comic Text") || previewRequested) {
		status_ = system.Play(preset_, previewPosition) ? "Preview started." : "Preview failed.";
	}
	ImGui::SameLine();
	ImGui::TextDisabled("Included in the composed hit preview when enabled.");
	ImGui::SeparatorText("Runtime usage");
	ImGui::TextWrapped("comicTextEffects.Play(\"%s\", hitWorldPosition);", presetName_.data());
}

#endif
