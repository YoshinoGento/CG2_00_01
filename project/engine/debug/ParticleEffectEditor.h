#pragma once

#ifdef USE_IMGUI

#include "effect/ParticleManager.h"

#include <array>
#include <string>

class ParticleEffectEditor final {
public:
	ParticleEffectEditor();

	void Draw(ParticleManager& particleManager, const Vector3& defaultPosition, float deltaTime);

private:
	void ApplyMagneticNovaPreset();
	void Emit(ParticleManager& particleManager);
	bool SavePreset();
	bool LoadPreset();
	std::string BuildPresetPath() const;

	GPUParticleEmitSettings settings_{};
	std::array<char, 64> presetName_{};
	std::string status_;
	float autoEmitTimer_ = 0.0f;
	float autoEmitInterval_ = 0.8f;
	bool autoEmit_ = false;
	bool useScenePosition_ = true;
};

#endif
