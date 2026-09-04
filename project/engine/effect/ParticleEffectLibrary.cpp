#include "effect/ParticleEffectLibrary.h"

#include "base/Logger.h"
#include "io/JsonFile.h"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace {
constexpr const char* kPresetDirectory = "Settings/effects/";

bool ReadVector3(const nlohmann::json& json, const char* key, Vector3& value) {
	if (!json.contains(key) || !json.at(key).is_array() || json.at(key).size() != 3) {
		return false;
	}
	value = {
		json.at(key).at(0).get<float>(),
		json.at(key).at(1).get<float>(),
		json.at(key).at(2).get<float>()
	};
	return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

bool ReadVector4(const nlohmann::json& json, const char* key, Vector4& value) {
	if (!json.contains(key) || !json.at(key).is_array() || json.at(key).size() != 4) {
		return false;
	}
	value = {
		json.at(key).at(0).get<float>(),
		json.at(key).at(1).get<float>(),
		json.at(key).at(2).get<float>(),
		json.at(key).at(3).get<float>()
	};
	return std::isfinite(value.x) && std::isfinite(value.y) &&
		std::isfinite(value.z) && std::isfinite(value.w);
}
}

void ParticleEffectLibrary::Initialize(ParticleManager* particleManager) {
	particleManager_ = particleManager;
	presets_.clear();
}

void ParticleEffectLibrary::Finalize() {
	presets_.clear();
	particleManager_ = nullptr;
}

bool ParticleEffectLibrary::IsSafePresetName(const std::string& presetName) {
	if (presetName.empty()) {
		return false;
	}
	for (const unsigned char ch : presetName) {
		if (!(std::isalnum(ch) || ch == '_' || ch == '-')) {
			return false;
		}
	}
	return true;
}

std::string ParticleEffectLibrary::BuildPresetPath(const std::string& presetName) {
	return std::string(kPresetDirectory) + presetName + ".json";
}

bool ParticleEffectLibrary::LoadFromFile(
	const std::string& presetName,
	GPUParticleEmitSettings& settings) const {
	if (!IsSafePresetName(presetName)) {
		Logger::Log("ParticleEffectLibrary rejected unsafe preset name: " + presetName);
		return false;
	}

	nlohmann::json json;
	const std::string path = BuildPresetPath(presetName);
	if (!JsonFile::Load(path, json)) {
		return false;
	}

	try {
		GPUParticleEmitSettings loaded{};
		if (!ReadVector4(json, "color", loaded.color) ||
			!ReadVector3(json, "scale", loaded.scale) ||
			!ReadVector3(json, "baseVelocity", loaded.baseVelocity)) {
			Logger::Log("ParticleEffectLibrary found invalid vectors: " + path);
			return false;
		}
		loaded.radius = json.value("radius", 1.0f);
		loaded.lifeTime = json.value("lifeTime", 1.0f);
		loaded.speed = json.value("speed", 0.5f);
		loaded.count = json.value("count", 128u);
		loaded.preset = json.value("gpuPreset", 0u);
		loaded.extendedSettings = 1;
		loaded.direction = { 0.0f, 1.0f, 0.0f };
		loaded.acceleration = {};
		loaded.endScale = loaded.scale;
		loaded.endAlpha = loaded.color.w;
		loaded.colorVariance = {};
		ReadVector3(json, "direction", loaded.direction);
		ReadVector3(json, "acceleration", loaded.acceleration);
		ReadVector3(json, "endScale", loaded.endScale);
		ReadVector4(json, "colorVariance", loaded.colorVariance);
		loaded.directionSpread = json.value("directionSpread", 3.14159265f);
		loaded.drag = json.value("drag", 0.0f);
		loaded.endAlpha = json.value("endAlpha", loaded.color.w);
		loaded.lifeTimeVariance = json.value("lifeTimeVariance", 0.0f);
		loaded.speedVariance = json.value("speedVariance", 0.0f);
		loaded.scaleVariance = json.value("scaleVariance", 0.0f);
		loaded.innerRadius = json.value("innerRadius", 0.0f);
		loaded.shape = json.value("shape", 0u);
		loaded.randomSeed = json.value("randomSeed", 1u);
		loaded.fadeMode = json.value("fadeMode", 0u);
		if (!std::isfinite(loaded.radius) || !std::isfinite(loaded.lifeTime) ||
			!std::isfinite(loaded.speed)) {
			Logger::Log("ParticleEffectLibrary found non-finite values: " + path);
			return false;
		}
		loaded.radius = std::clamp(loaded.radius, 0.0f, 20.0f);
		loaded.lifeTime = std::clamp(loaded.lifeTime, 0.05f, 20.0f);
		loaded.speed = std::clamp(loaded.speed, 0.0f, 10.0f);
		loaded.innerRadius = std::clamp(loaded.innerRadius, 0.0f, loaded.radius);
		loaded.drag = std::clamp(loaded.drag, 0.0f, 20.0f);
		loaded.shape = (std::min)(loaded.shape, 3u);
		loaded.fadeMode = (std::min)(loaded.fadeMode, 2u);
		loaded.count = std::clamp(loaded.count, 1u, 1024u);
		loaded.emit = 1;
		settings = loaded;
	} catch (const nlohmann::json::exception& exception) {
		Logger::Log("ParticleEffectLibrary failed to parse " + path + ": " + exception.what());
		return false;
	}
	return true;
}

bool ParticleEffectLibrary::Play(const std::string& presetName, const Vector3& position) {
	if (!particleManager_ || !std::isfinite(position.x) ||
		!std::isfinite(position.y) || !std::isfinite(position.z)) {
		return false;
	}
	auto found = presets_.find(presetName);
	if (found == presets_.end()) {
		GPUParticleEmitSettings loaded{};
		if (!LoadFromFile(presetName, loaded)) {
			return false;
		}
		found = presets_.emplace(presetName, loaded).first;
	}

	GPUParticleEmitSettings settings = found->second;
	settings.translate = position;
	settings.emit = 1;
	particleManager_->RequestGPUParticleEmit(settings);
	return true;
}

bool ParticleEffectLibrary::Reload(const std::string& presetName) {
	GPUParticleEmitSettings loaded{};
	if (!LoadFromFile(presetName, loaded)) {
		return false;
	}
	presets_[presetName] = loaded;
	return true;
}

void ParticleEffectLibrary::ClearCache() {
	presets_.clear();
}
