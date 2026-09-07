#include "application/magnet/system/MagneticOneShotSoundSystem.h"

#include <algorithm>
#include <cmath>

namespace magnet {

bool MagneticOneShotSoundSystem::Initialize(
	Audio* audio,
	const char* resourcePath,
	const Settings& settings)
{
	Finalize();
	if (!audio || !resourcePath || resourcePath[0] == '\0') {
		return false;
	}

	settings_.volume = std::isfinite(settings.volume)
		? std::clamp(settings.volume, 0.0f, 1.0f)
		: 0.62f;
	settings_.playbackRate = std::isfinite(settings.playbackRate)
		? std::clamp(settings.playbackRate, 0.5f, 2.0f)
		: 1.0f;

	audio_ = audio;
	clip_ = audio_->LoadAudio(resourcePath);
	if (!clip_) {
		Finalize();
		return false;
	}
	return true;
}

void MagneticOneShotSoundSystem::Finalize()
{
	Reset();
	audio_ = nullptr;
	clip_ = {};
}

void MagneticOneShotSoundSystem::Reset()
{
	if (audio_ && activeVoice_) {
		(void)audio_->StopVoice(activeVoice_);
	}
	activeVoice_ = {};
}

void MagneticOneShotSoundSystem::Play()
{
	if (!IsReady()) {
		return;
	}
	if (activeVoice_) {
		(void)audio_->StopVoice(activeVoice_);
		activeVoice_ = {};
	}

	Audio::PlaySettings playSettings{};
	playSettings.volume = settings_.volume;
	playSettings.playbackRate = settings_.playbackRate;
	activeVoice_ = audio_->Play(clip_, playSettings);
}

} // namespace magnet
