#include "application/magnet/system/MagneticImpactSoundSystem.h"

#include <algorithm>
#include <cmath>

namespace magnet {

bool MagneticImpactSoundSystem::Initialize(
	Audio* audio,
	const char* resourcePath,
	const Settings& settings)
{
	Finalize();
	if (!audio || !resourcePath || resourcePath[0] == '\0') {
		return false;
	}
	settings_.minimumImpactSpeed = std::isfinite(settings.minimumImpactSpeed)
		? std::clamp(settings.minimumImpactSpeed, 0.0f, 80.0f) : 2.0f;
	settings_.fullVolumeImpactSpeed = std::isfinite(settings.fullVolumeImpactSpeed)
		? std::clamp(
			settings.fullVolumeImpactSpeed,
			settings_.minimumImpactSpeed + 0.01f,
			80.0f)
		: 14.0f;
	settings_.minimumVolume = std::isfinite(settings.minimumVolume)
		? std::clamp(settings.minimumVolume, 0.0f, 1.0f) : 0.28f;
	settings_.maximumVolume = std::isfinite(settings.maximumVolume)
		? std::clamp(
			settings.maximumVolume,
			settings_.minimumVolume,
			1.0f)
		: 0.82f;
	settings_.minimumPlaybackRate = std::isfinite(settings.minimumPlaybackRate)
		? std::clamp(settings.minimumPlaybackRate, 0.5f, 2.0f) : 0.94f;
	settings_.maximumPlaybackRate = std::isfinite(settings.maximumPlaybackRate)
		? std::clamp(
			settings.maximumPlaybackRate,
			settings_.minimumPlaybackRate,
			2.0f)
		: 1.04f;
	settings_.retriggerCooldownSeconds =
		std::isfinite(settings.retriggerCooldownSeconds)
		? std::clamp(settings.retriggerCooldownSeconds, 0.0f, 2.0f)
		: 0.22f;

	audio_ = audio;
	clip_ = audio_->LoadAudio(resourcePath);
	if (!clip_) {
		Finalize();
		return false;
	}
	Reset();
	return true;
}

void MagneticImpactSoundSystem::Finalize() noexcept
{
	audio_ = nullptr;
	clip_ = {};
	cooldownRemainingSeconds_ = 0.0f;
	strongestPendingImpactSpeed_ = 0.0f;
}

void MagneticImpactSoundSystem::Reset() noexcept
{
	cooldownRemainingSeconds_ = 0.0f;
	strongestPendingImpactSpeed_ = 0.0f;
}

void MagneticImpactSoundSystem::BeginFixedUpdate(float fixedDeltaTime) noexcept
{
	strongestPendingImpactSpeed_ = 0.0f;
	if (!std::isfinite(fixedDeltaTime) || fixedDeltaTime <= 0.0f) {
		return;
	}
	cooldownRemainingSeconds_ = (std::max)(
		0.0f,
		cooldownRemainingSeconds_ - fixedDeltaTime);
}

void MagneticImpactSoundSystem::AddImpact(float relativeSpeed) noexcept
{
	if (!std::isfinite(relativeSpeed) ||
		relativeSpeed < settings_.minimumImpactSpeed) {
		return;
	}
	strongestPendingImpactSpeed_ = (std::max)(
		strongestPendingImpactSpeed_,
		relativeSpeed);
}

void MagneticImpactSoundSystem::PlayPending()
{
	if (!IsReady() || cooldownRemainingSeconds_ > 0.0f ||
		strongestPendingImpactSpeed_ < settings_.minimumImpactSpeed) {
		strongestPendingImpactSpeed_ = 0.0f;
		return;
	}
	const float intensity = std::clamp(
		(strongestPendingImpactSpeed_ - settings_.minimumImpactSpeed) /
		(settings_.fullVolumeImpactSpeed - settings_.minimumImpactSpeed),
		0.0f,
		1.0f);
	Audio::PlaySettings playSettings{};
	playSettings.volume = std::lerp(
		settings_.minimumVolume,
		settings_.maximumVolume,
		intensity);
	playSettings.playbackRate = std::lerp(
		settings_.minimumPlaybackRate,
		settings_.maximumPlaybackRate,
		intensity);
	if (audio_->Play(clip_, playSettings)) {
		cooldownRemainingSeconds_ = settings_.retriggerCooldownSeconds;
	}
	strongestPendingImpactSpeed_ = 0.0f;
}

} // namespace magnet
