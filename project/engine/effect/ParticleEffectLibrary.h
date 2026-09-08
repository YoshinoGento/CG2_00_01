#pragma once

#include "effect/ParticleManager.h"

#include <map>
#include <string>

// JSONで作成したパーティクルプリセットを名前と座標だけで再生する共通窓口。
class ParticleEffectLibrary final {
public:
	void Initialize(ParticleManager* particleManager);
	void Finalize();

	[[nodiscard]] bool Play(const std::string& presetName, const Vector3& position);
	[[nodiscard]] bool Reload(const std::string& presetName);
	void ClearCache();

private:
	[[nodiscard]] bool LoadFromFile(
		const std::string& presetName,
		GPUParticleEmitSettings& settings) const;
	[[nodiscard]] static bool IsSafePresetName(const std::string& presetName);
	[[nodiscard]] static std::string BuildPresetPath(const std::string& presetName);

	ParticleManager* particleManager_ = nullptr;
	std::map<std::string, GPUParticleEmitSettings> presets_;
};
