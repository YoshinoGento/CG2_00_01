#include "application/magnet/system/ChainsawProximitySoundSystem.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace magnet {
namespace {

constexpr float kMinimumDistanceRange = 0.01f;
constexpr float kMinimumVolumeResponse = 0.01f;
constexpr float kSilentVolume = 0.0005f;

[[nodiscard]] bool IsFiniteVector3(const Vector3& value) noexcept
{
	return std::isfinite(value.x) && std::isfinite(value.y) &&
		std::isfinite(value.z);
}

} // namespace

bool ChainsawProximitySoundSystem::Initialize(
	Audio* audio,
	const char* resourcePath,
	const Settings& settings)
{
	Finalize();
	if (!audio || !resourcePath || resourcePath[0] == '\0') {
		return false;
	}

	const float fullVolumeDistance = std::isfinite(settings.fullVolumeDistance)
		? std::clamp(
			settings.fullVolumeDistance,
			0.0f,
			100.0f - kMinimumDistanceRange)
		: 2.5f;
	settings_.fullVolumeDistance = fullVolumeDistance;
	settings_.audibleDistance = std::isfinite(settings.audibleDistance)
		? std::clamp(
			settings.audibleDistance,
			fullVolumeDistance + kMinimumDistanceRange,
			100.0f)
		: 10.0f;
	settings_.maximumVolume = std::isfinite(settings.maximumVolume)
		? std::clamp(settings.maximumVolume, 0.0f, 1.0f)
		: 0.32f;
	settings_.playbackRate = std::isfinite(settings.playbackRate)
		? std::clamp(settings.playbackRate, 0.5f, 2.0f)
		: 1.0f;
	settings_.volumeResponse = std::isfinite(settings.volumeResponse)
		? std::clamp(settings.volumeResponse, kMinimumVolumeResponse, 60.0f)
		: 7.0f;

	audio_ = audio;
	clip_ = audio_->LoadAudio(resourcePath);
	if (!clip_) {
		Finalize();
		return false;
	}
	return true;
}

void ChainsawProximitySoundSystem::Finalize()
{
	Reset();
	audio_ = nullptr;
	clip_ = {};
}

void ChainsawProximitySoundSystem::Reset()
{
	if (audio_ && loopVoice_) {
		(void)audio_->StopVoice(loopVoice_);
	}
	loopVoice_ = {};
	currentVolume_ = 0.0f;
}

void ChainsawProximitySoundSystem::Update(
	float deltaTime,
	bool enabled,
	const Vector3& playerPosition,
	const MagnetStageBoxPlacement* obstacles,
	std::size_t obstacleCount) noexcept
{
	if (!IsReady() || !std::isfinite(deltaTime) || deltaTime < 0.0f) {
		return;
	}

	const float targetVolume = enabled
		? CalculateTargetVolume(playerPosition, obstacles, obstacleCount)
		: 0.0f;
	if (targetVolume > 0.0f && !EnsureVoice()) {
		return;
	}

	const float response = 1.0f - std::exp(-settings_.volumeResponse * deltaTime);
	currentVolume_ += (targetVolume - currentVolume_) * response;
	if (targetVolume <= 0.0f && currentVolume_ <= kSilentVolume) {
		Reset();
		return;
	}
	if (!loopVoice_ || !audio_->SetVoiceVolume(loopVoice_, currentVolume_)) {
		if (loopVoice_) {
			(void)audio_->StopVoice(loopVoice_);
		}
		loopVoice_ = {};
		currentVolume_ = 0.0f;
	}
}

float ChainsawProximitySoundSystem::CalculateTargetVolume(
	const Vector3& playerPosition,
	const MagnetStageBoxPlacement* obstacles,
	std::size_t obstacleCount) const noexcept
{
	if (!IsFiniteVector3(playerPosition) ||
		(!obstacles && obstacleCount > 0) ||
		obstacleCount > MagnetStageData::kMaximumObstacleCount) {
		return 0.0f;
	}

	float nearestDistanceSquared = (std::numeric_limits<float>::max)();
	for (std::size_t index = 0; index < obstacleCount; ++index) {
		const MagnetStageBoxPlacement& obstacle = obstacles[index];
		if (obstacle.obstacleKind != MagnetObstacleKind::Chainsaw ||
			!IsFiniteVector3(obstacle.position) || !IsFiniteVector3(obstacle.size)) {
			continue;
		}
		const float halfWidth = (std::max)(std::fabs(obstacle.size.x) * 0.5f, 0.0f);
		const float halfDepth = (std::max)(std::fabs(obstacle.size.z) * 0.5f, 0.0f);
		const float distanceX = (std::max)(
			std::fabs(playerPosition.x - obstacle.position.x) - halfWidth, 0.0f);
		const float distanceZ = (std::max)(
			std::fabs(playerPosition.z - obstacle.position.z) - halfDepth, 0.0f);
		nearestDistanceSquared = (std::min)(
			nearestDistanceSquared,
			distanceX * distanceX + distanceZ * distanceZ);
	}
	if (nearestDistanceSquared == (std::numeric_limits<float>::max)()) {
		return 0.0f;
	}

	const float distance = std::sqrt(nearestDistanceSquared);
	const float normalized = std::clamp(
		(settings_.audibleDistance - distance) /
			(settings_.audibleDistance - settings_.fullVolumeDistance),
		0.0f,
		1.0f);
	const float smoothDistance = normalized * normalized * (3.0f - 2.0f * normalized);
	return settings_.maximumVolume * smoothDistance;
}

bool ChainsawProximitySoundSystem::EnsureVoice() noexcept
{
	if (loopVoice_ && audio_->IsVoiceActive(loopVoice_)) {
		return true;
	}
	loopVoice_ = {};
	Audio::PlaySettings playSettings{};
	playSettings.loop = true;
	playSettings.volume = 0.0f;
	playSettings.playbackRate = settings_.playbackRate;
	loopVoice_ = audio_->Play(clip_, playSettings);
	return loopVoice_.IsValid();
}

} // namespace magnet
