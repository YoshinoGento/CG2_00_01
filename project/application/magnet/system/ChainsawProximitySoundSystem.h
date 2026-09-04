#pragma once

#include "application/magnet/stage/MagnetStageSystem.h"
#include "audio/Audio.h"

#include <cstddef>

namespace magnet {

// Owns the single looping Chainsaw Voice for the current scene.
// Stage data and Player position are borrowed only for the duration of Update.
class ChainsawProximitySoundSystem final {
public:
	struct Settings {
		float audibleDistance = 10.0f;
		float fullVolumeDistance = 2.5f;
		float maximumVolume = 0.32f;
		float playbackRate = 1.0f;
		float volumeResponse = 7.0f;
	};

	[[nodiscard]] bool Initialize(
		Audio* audio,
		const char* resourcePath,
		const Settings& settings = {});
	void Finalize();
	void Reset();
	void Update(
		float deltaTime,
		bool enabled,
		const Vector3& playerPosition,
		const MagnetStageBoxPlacement* obstacles,
		std::size_t obstacleCount) noexcept;
	[[nodiscard]] bool IsReady() const noexcept {
		return audio_ != nullptr && clip_.IsValid();
	}
	[[nodiscard]] float GetCurrentVolume() const noexcept { return currentVolume_; }

private:
	[[nodiscard]] float CalculateTargetVolume(
		const Vector3& playerPosition,
		const MagnetStageBoxPlacement* obstacles,
		std::size_t obstacleCount) const noexcept;
	[[nodiscard]] bool EnsureVoice() noexcept;

	Audio* audio_ = nullptr;
	AudioClipHandle clip_{};
	AudioVoiceHandle loopVoice_{};
	Settings settings_{};
	float currentVolume_ = 0.0f;
};

} // namespace magnet
