#pragma once

#include "audio/Audio.h"

namespace magnet {

// Owns one replaceable gameplay one-shot Voice for a scene-owned sound role.
// Audio owns the decoded clip and outlives this System.
class MagneticOneShotSoundSystem final {
public:
	struct Settings {
		float volume = 0.62f;
		float playbackRate = 1.0f;
	};

	[[nodiscard]] bool Initialize(
		Audio* audio,
		const char* resourcePath,
		const Settings& settings = {});
	void Finalize();
	void Reset();
	void Play();
	[[nodiscard]] bool IsReady() const noexcept {
		return audio_ != nullptr && clip_.IsValid();
	}

private:
	Audio* audio_ = nullptr;
	AudioClipHandle clip_{};
	AudioVoiceHandle activeVoice_{};
	Settings settings_{};
};

} // namespace magnet
