#include "debug/ParticleEffectEditor.h"

#ifdef USE_IMGUI

#include "externals/imgui/imgui.h"
#include "base/Framework.h"
#include "effect/ParticleEffectLibrary.h"
#include "io/JsonFile.h"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <filesystem>

namespace {
constexpr const char* kPresetDirectory = "Settings/effects/";

bool IsSafePresetName(const char* name) {
	if (!name || name[0] == '\0') {
		return false;
	}
	for (const unsigned char ch : std::string(name)) {
		if (!(std::isalnum(ch) || ch == '_' || ch == '-')) {
			return false;
		}
	}
	return true;
}

float FiniteOr(float value, float fallback) {
	return std::isfinite(value) ? value : fallback;
}

std::string PathComponentUtf8(const std::filesystem::path& path) {
	const std::u8string value = path.u8string();
	return { reinterpret_cast<const char*>(value.data()), value.size() };
}
}

ParticleEffectEditor::ParticleEffectEditor() {
	strcpy_s(presetName_.data(), presetName_.size(), "MagneticNova");
	ApplyMagneticNovaPreset();
	RefreshPresetList();
}

void ParticleEffectEditor::ApplyMagneticNovaPreset() {
	settings_ = {};
	settings_.translate = { 0.0f, 1.0f, 10.0f };
	settings_.radius = 1.4f;
	settings_.color = { 0.28f, 0.72f, 1.0f, 1.0f };
	settings_.scale = { 0.18f, 0.18f, 0.18f };
	settings_.lifeTime = 1.35f;
	settings_.baseVelocity = { 0.0f, 0.04f, 0.0f };
	settings_.speed = 0.85f;
	settings_.count = 320;
	settings_.emit = 1;
	settings_.preset = 0;
	settings_.extendedSettings = 1;
	settings_.direction = { 0.0f, 1.0f, 0.0f };
	settings_.directionSpread = 0.85f;
	settings_.acceleration = { 0.0f, -0.18f, 0.0f };
	settings_.drag = 0.35f;
	settings_.endScale = { 0.02f, 0.02f, 0.02f };
	settings_.endAlpha = 0.0f;
	settings_.colorVariance = { 0.18f, 0.12f, 0.20f, 0.0f };
	settings_.lifeTimeVariance = 0.35f;
	settings_.speedVariance = 0.28f;
	settings_.scaleVariance = 0.35f;
	settings_.innerRadius = 0.15f;
	settings_.shape = 0;
	settings_.randomSeed = 1;
	settings_.fadeMode = 2;
	status_ = "Magnetic Nova preset applied";
}

void ParticleEffectEditor::Emit(ParticleManager& particleManager) {
	settings_.radius = std::clamp(FiniteOr(settings_.radius, 1.0f), 0.0f, 20.0f);
	settings_.lifeTime = std::clamp(FiniteOr(settings_.lifeTime, 1.0f), 0.05f, 20.0f);
	settings_.speed = std::clamp(FiniteOr(settings_.speed, 0.5f), 0.0f, 10.0f);
	settings_.count = std::clamp(settings_.count, 1u, 1024u);
	settings_.innerRadius = std::clamp(FiniteOr(settings_.innerRadius, 0.0f), 0.0f, settings_.radius);
	settings_.drag = std::clamp(FiniteOr(settings_.drag, 0.0f), 0.0f, 20.0f);
	settings_.extendedSettings = 1;
	settings_.emit = 1;
	particleManager.RequestGPUParticleEmit(settings_);
}

std::string ParticleEffectEditor::BuildPresetPath() const {
	if (!IsSafePresetName(presetName_.data())) {
		return {};
	}
	return std::string(kPresetDirectory) + presetName_.data() + ".json";
}

bool ParticleEffectEditor::SavePreset(bool overwrite) {
	const std::string path = BuildPresetPath();
	if (path.empty()) {
		status_ = "Save failed: use only A-Z, 0-9, _ or -";
		return false;
	}
	std::error_code existsError;
	const bool presetExists = std::filesystem::exists(std::filesystem::path(path), existsError);
	if (existsError) {
		status_ = "Save failed: could not inspect preset path.";
		return false;
	}
	if (!overwrite && presetExists) {
		status_ = "Save failed: preset already exists. Use Overwrite.";
		return false;
	}

	nlohmann::json json = {
		{ "version", 1 },
		{ "name", presetName_.data() },
		{ "position", { settings_.translate.x, settings_.translate.y, settings_.translate.z } },
		{ "radius", settings_.radius },
		{ "color", { settings_.color.x, settings_.color.y, settings_.color.z, settings_.color.w } },
		{ "scale", { settings_.scale.x, settings_.scale.y, settings_.scale.z } },
		{ "lifeTime", settings_.lifeTime },
		{ "baseVelocity", { settings_.baseVelocity.x, settings_.baseVelocity.y, settings_.baseVelocity.z } },
		{ "speed", settings_.speed },
		{ "count", settings_.count },
		{ "gpuPreset", settings_.preset }
		,{ "shape", settings_.shape }
		,{ "direction", { settings_.direction.x, settings_.direction.y, settings_.direction.z } }
		,{ "directionSpread", settings_.directionSpread }
		,{ "acceleration", { settings_.acceleration.x, settings_.acceleration.y, settings_.acceleration.z } }
		,{ "drag", settings_.drag }
		,{ "endScale", { settings_.endScale.x, settings_.endScale.y, settings_.endScale.z } }
		,{ "endAlpha", settings_.endAlpha }
		,{ "colorVariance", { settings_.colorVariance.x, settings_.colorVariance.y, settings_.colorVariance.z, settings_.colorVariance.w } }
		,{ "lifeTimeVariance", settings_.lifeTimeVariance }
		,{ "speedVariance", settings_.speedVariance }
		,{ "scaleVariance", settings_.scaleVariance }
		,{ "innerRadius", settings_.innerRadius }
		,{ "randomSeed", settings_.randomSeed }
		,{ "fadeMode", settings_.fadeMode }
	};
	std::error_code directoryError;
	std::filesystem::create_directories(kPresetDirectory, directoryError);
	if (directoryError) {
		status_ = "Save failed: could not create Settings/effects";
		return false;
	}
	if (!JsonFile::Save(path, json)) {
		status_ = "Save failed: " + path;
		return false;
	}
	if (Framework* framework = Framework::GetInstance()) {
		if (ParticleEffectLibrary* library = framework->GetParticleEffectLibrary()) {
			if (!library->Reload(presetName_.data())) {
				status_ = "Saved, but runtime cache reload failed: " + path;
				return true;
			}
		}
	}
	status_ = "Saved: " + path;
	RefreshPresetList();
	return true;
}

void ParticleEffectEditor::RefreshPresetList() {
	presetNames_.clear();
	std::error_code error;
	if (!std::filesystem::exists(kPresetDirectory, error)) {
		selectedPresetIndex_ = -1;
		return;
	}
	for (const std::filesystem::directory_entry& entry :
		std::filesystem::directory_iterator(kPresetDirectory, error)) {
		if (error) {
			break;
		}
		if (entry.is_regular_file(error) && entry.path().extension() == ".json") {
			const std::string name = PathComponentUtf8(entry.path().stem());
			if (IsSafePresetName(name.c_str())) {
				presetNames_.push_back(name);
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

void ParticleEffectEditor::SelectPreset(int index) {
	if (index < 0 || index >= static_cast<int>(presetNames_.size())) {
		return;
	}
	selectedPresetIndex_ = index;
	strncpy_s(presetName_.data(), presetName_.size(), presetNames_[index].c_str(), _TRUNCATE);
	LoadPreset();
}

void ParticleEffectEditor::DeleteSelectedPreset() {
	if (selectedPresetIndex_ < 0 || selectedPresetIndex_ >= static_cast<int>(presetNames_.size())) {
		status_ = "Delete failed: select a preset first.";
		return;
	}
	const std::string name = presetNames_[selectedPresetIndex_];
	const std::filesystem::path path = std::filesystem::path(kPresetDirectory) / (name + ".json");
	std::error_code error;
	if (!std::filesystem::remove(path, error) || error) {
		status_ = "Delete failed: " + PathComponentUtf8(path);
		return;
	}
	if (Framework* framework = Framework::GetInstance()) {
		if (ParticleEffectLibrary* library = framework->GetParticleEffectLibrary()) {
			library->ClearCache();
		}
	}
	status_ = "Deleted: " + PathComponentUtf8(path);
	RefreshPresetList();
}

bool ParticleEffectEditor::LoadPreset() {
	const std::string path = BuildPresetPath();
	if (path.empty()) {
		status_ = "Load failed: use only A-Z, 0-9, _ or -";
		return false;
	}

	nlohmann::json json;
	if (!JsonFile::Load(path, json)) {
		status_ = "Load failed: " + path;
		return false;
	}
	try {
	{
		const auto position = json.at("position");
		const auto color = json.at("color");
		const auto scale = json.at("scale");
		const auto velocity = json.at("baseVelocity");
		settings_.translate = { position.at(0).get<float>(), position.at(1).get<float>(), position.at(2).get<float>() };
		settings_.color = { color.at(0).get<float>(), color.at(1).get<float>(), color.at(2).get<float>(), color.at(3).get<float>() };
		settings_.scale = { scale.at(0).get<float>(), scale.at(1).get<float>(), scale.at(2).get<float>() };
		settings_.baseVelocity = { velocity.at(0).get<float>(), velocity.at(1).get<float>(), velocity.at(2).get<float>() };
		settings_.radius = json.value("radius", 1.0f);
		settings_.lifeTime = json.value("lifeTime", 1.0f);
		settings_.speed = json.value("speed", 0.5f);
		settings_.count = json.value("count", 128u);
		settings_.preset = json.value("gpuPreset", 0u);
		settings_.extendedSettings = 1;
		settings_.shape = json.value("shape", 0u);
		settings_.directionSpread = json.value("directionSpread", 3.14159265f);
		settings_.drag = json.value("drag", 0.0f);
		settings_.endAlpha = json.value("endAlpha", settings_.color.w);
		settings_.lifeTimeVariance = json.value("lifeTimeVariance", 0.0f);
		settings_.speedVariance = json.value("speedVariance", 0.0f);
		settings_.scaleVariance = json.value("scaleVariance", 0.0f);
		settings_.innerRadius = json.value("innerRadius", 0.0f);
		settings_.randomSeed = json.value("randomSeed", 1u);
		settings_.fadeMode = json.value("fadeMode", 0u);
		settings_.direction = { 0.0f, 1.0f, 0.0f };
		settings_.acceleration = {};
		settings_.endScale = settings_.scale;
		settings_.colorVariance = {};
		if (json.contains("direction")) {
			const auto value = json.at("direction");
			settings_.direction = { value.at(0).get<float>(), value.at(1).get<float>(), value.at(2).get<float>() };
		}
		if (json.contains("acceleration")) {
			const auto value = json.at("acceleration");
			settings_.acceleration = { value.at(0).get<float>(), value.at(1).get<float>(), value.at(2).get<float>() };
		}
		if (json.contains("endScale")) {
			const auto value = json.at("endScale");
			settings_.endScale = { value.at(0).get<float>(), value.at(1).get<float>(), value.at(2).get<float>() };
		}
		if (json.contains("colorVariance")) {
			const auto value = json.at("colorVariance");
			settings_.colorVariance = { value.at(0).get<float>(), value.at(1).get<float>(), value.at(2).get<float>(), value.at(3).get<float>() };
		}
	}
	} catch (const nlohmann::json::exception&) {
		status_ = "Load failed: invalid preset data";
		return false;
	}
	status_ = "Loaded: " + path;
	return true;
}

void ParticleEffectEditor::Draw(ParticleManager& particleManager, const Vector3& defaultPosition, float deltaTime) {
	if (useScenePosition_) {
		settings_.translate = defaultPosition;
	}

	ImGui::SeparatorText("Particle Effect Editor");
	ImGui::Checkbox("Use Scene Sphere Position", &useScenePosition_);
	if (!useScenePosition_) {
		ImGui::DragFloat3("Editor Position", &settings_.translate.x, 0.05f, -100.0f, 100.0f);
	}
	if (ImGui::CollapsingHeader("Emission", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::ColorEdit4("Effect Color", &settings_.color.x);
	int emitCount = static_cast<int>(settings_.count);
	if (ImGui::SliderInt("Emit Count", &emitCount, 1, 1024)) {
		settings_.count = static_cast<uint32_t>(emitCount);
	}
	ImGui::SliderFloat("Spawn Radius", &settings_.radius, 0.0f, 10.0f);
	ImGui::SliderFloat("Inner Radius", &settings_.innerRadius, 0.0f, settings_.radius);
	int shape = static_cast<int>(settings_.shape);
	const char* shapes[] = { "Sphere", "Hemisphere", "Cone", "Ring" };
	if (ImGui::Combo("Emitter Shape", &shape, shapes, IM_ARRAYSIZE(shapes))) {
		settings_.shape = static_cast<uint32_t>(shape);
	}
	if (settings_.shape == 2) {
		ImGui::DragFloat3("Cone Direction", &settings_.direction.x, 0.02f, -1.0f, 1.0f);
		ImGui::SliderAngle("Cone Spread", &settings_.directionSpread, 0.0f, 180.0f);
	}
	}
	if (ImGui::CollapsingHeader("Motion", ImGuiTreeNodeFlags_DefaultOpen)) {
	ImGui::SliderFloat3("Particle Scale", &settings_.scale.x, 0.01f, 2.0f);
	ImGui::SliderFloat3("End Scale", &settings_.endScale.x, 0.0f, 2.0f);
	ImGui::SliderFloat("Life Time", &settings_.lifeTime, 0.05f, 10.0f);
	ImGui::DragFloat3("Base Velocity", &settings_.baseVelocity.x, 0.01f, -2.0f, 2.0f);
	ImGui::SliderFloat("Spread Speed", &settings_.speed, 0.0f, 5.0f);
	ImGui::DragFloat3("Acceleration / Gravity", &settings_.acceleration.x, 0.01f, -5.0f, 5.0f);
	ImGui::SliderFloat("Drag", &settings_.drag, 0.0f, 10.0f);
	}
	if (ImGui::CollapsingHeader("Lifetime & Fade", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::SliderFloat("End Alpha", &settings_.endAlpha, 0.0f, 1.0f);
		int fadeMode = static_cast<int>(settings_.fadeMode);
		const char* fadeModes[] = { "Linear", "Ease Out", "Smooth" };
		if (ImGui::Combo("Fade Curve", &fadeMode, fadeModes, IM_ARRAYSIZE(fadeModes))) {
			settings_.fadeMode = static_cast<uint32_t>(fadeMode);
		}
	}
	if (ImGui::CollapsingHeader("Randomness", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::SliderFloat("Lifetime Variation", &settings_.lifeTimeVariance, 0.0f, 5.0f);
		ImGui::SliderFloat("Speed Variation", &settings_.speedVariance, 0.0f, 5.0f);
		ImGui::SliderFloat("Scale Variation", &settings_.scaleVariance, 0.0f, 1.0f);
		ImGui::SliderFloat3("RGB Variation", &settings_.colorVariance.x, 0.0f, 1.0f);
		int randomSeed = static_cast<int>(settings_.randomSeed);
		if (ImGui::InputInt("Random Seed", &randomSeed)) {
			settings_.randomSeed = static_cast<uint32_t>((std::max)(randomSeed, 0));
		}
	}

	if (ImGui::Button("Preview Effect", ImVec2(150.0f, 0.0f))) {
		Emit(particleManager);
	}
	ImGui::SameLine();
	if (ImGui::Button("Magnetic Nova", ImVec2(150.0f, 0.0f))) {
		ApplyMagneticNovaPreset();
		if (useScenePosition_) {
			settings_.translate = defaultPosition;
		}
		Emit(particleManager);
	}

	ImGui::Checkbox("Auto Preview", &autoEmit_);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(140.0f);
	ImGui::SliderFloat("Interval", &autoEmitInterval_, 0.1f, 5.0f);
	if (autoEmit_) {
		autoEmitTimer_ += std::clamp(FiniteOr(deltaTime, 0.0f), 0.0f, 0.25f);
		if (autoEmitTimer_ >= autoEmitInterval_) {
			autoEmitTimer_ = 0.0f;
			Emit(particleManager);
		}
	} else {
		autoEmitTimer_ = 0.0f;
	}

	ImGui::SeparatorText("Preset Library");
	if (ImGui::Button("Refresh List##Particle")) {
		RefreshPresetList();
	}
	ImGui::SameLine();
	ImGui::TextDisabled("%d preset(s)", static_cast<int>(presetNames_.size()));
	if (ImGui::BeginListBox("Saved Presets##Particle", ImVec2(-FLT_MIN, 110.0f))) {
		for (int index = 0; index < static_cast<int>(presetNames_.size()); ++index) {
			const bool selected = index == selectedPresetIndex_;
			if (ImGui::Selectable(presetNames_[index].c_str(), selected)) {
				SelectPreset(index);
			}
			if (selected) {
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndListBox();
	}
	ImGui::InputText("Preset Name", presetName_.data(), presetName_.size());
	if (ImGui::Button("Save New", ImVec2(110.0f, 0.0f))) {
		SavePreset(false);
	}
	ImGui::SameLine();
	if (ImGui::Button("Overwrite", ImVec2(110.0f, 0.0f))) {
		SavePreset(true);
	}
	ImGui::SameLine();
	if (ImGui::Button("Load", ImVec2(80.0f, 0.0f))) {
		LoadPreset();
	}
	ImGui::SameLine();
	if (ImGui::Button("Delete", ImVec2(80.0f, 0.0f))) {
		if (selectedPresetIndex_ >= 0 && selectedPresetIndex_ < static_cast<int>(presetNames_.size())) {
			pendingDeletePreset_ = presetNames_[selectedPresetIndex_];
			ImGui::OpenPopup("Delete Particle Preset?");
		} else {
			status_ = "Delete failed: select a preset first.";
		}
	}
	if (ImGui::BeginPopupModal("Delete Particle Preset?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::Text("Delete '%s'?", pendingDeletePreset_.c_str());
		ImGui::TextUnformatted("This cannot be undone.");
		if (ImGui::Button("Delete##ConfirmParticle", ImVec2(120.0f, 0.0f))) {
			DeleteSelectedPreset();
			pendingDeletePreset_.clear();
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel##Particle", ImVec2(120.0f, 0.0f))) {
			pendingDeletePreset_.clear();
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
	if (!status_.empty()) {
		ImGui::TextWrapped("%s", status_.c_str());
	}
}

#endif
