#pragma once

#include "audio/Audio.h"

namespace magnet {

// Selects one strongest impact per fixed step and rate-limits audio playback.
// Audio owns the decoded clip and outlives this scene-owned System.
class MagneticImpactSoundSystem final {
public:
	struct Settings {
		float minimumImpactSpeed = 2.0f;
		float fullVolumeImpactSpeed = 14.0f;
		float minimumVolume = 0.28f;
		float maximumVolume = 0.82f;
		float minimumPlaybackRate = 0.94f;
		float maximumPlaybackRate = 1.04f;
		float retriggerCooldownSeconds = 0.22f;
	};

	[[nodiscard]] bool Initialize(
		Audio* audio,
		const char* resourcePath,
		const Settings& settings = {});
	void Finalize() noexcept;
	void Reset() noexcept;
	void BeginFixedUpdate(float fixedDeltaTime) noexcept;
	void AddImpact(float relativeSpeed) noexcept;
	void PlayPending();
	[[nodiscard]] bool IsReady() const noexcept {
		return audio_ != nullptr && clip_.IsValid();
	}

private:
	Audio* audio_ = nullptr;
	AudioClipHandle clip_{};
	Settings settings_{};
	float cooldownRemainingSeconds_ = 0.0f;
	float strongestPendingImpactSpeed_ = 0.0f;
};

} // namespace magnet
