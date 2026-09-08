#pragma once

#include "audio/Audio.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <optional>

class GameFlowState final {
public:
	static constexpr std::size_t kRankingCapacity = 5;

	static GameFlowState& GetInstance() noexcept
	{
		static GameFlowState instance;
		return instance;
	}

	void EnsureBgm(Audio* audio)
	{
		if (!audio) { return; }
		audio_ = audio;
		audio_->SetSoundEffectVolume(seVolume_);
		if (!bgmClip_) { bgmClip_ = audio_->LoadAudio("Resources/bgm.wav"); }
		if (bgmClip_ && !audio_->IsVoiceActive(bgmVoice_)) {
			Audio::PlaySettings settings{};
			settings.loop = true;
			settings.useSoundEffectVolume = false;
			settings.volume = bgmVolume_;
			bgmVoice_ = audio_->Play(bgmClip_, settings);
		}
	}

	void SetBgmVolume(float volume)
	{
		bgmVolume_ = std::clamp(volume, 0.0f, 1.0f);
		if (audio_ && bgmVoice_) {
			(void)audio_->SetVoiceVolume(bgmVoice_, bgmVolume_);
		}
	}

	[[nodiscard]] float GetBgmVolume() const noexcept { return bgmVolume_; }

	void SetSeVolume(float volume)
	{
		seVolume_ = std::clamp(volume, 0.0f, 1.0f);
		if (audio_) { audio_->SetSoundEffectVolume(seVolume_); }
	}

	[[nodiscard]] float GetSeVolume() const noexcept { return seVolume_; }

	void SubmitScore(std::size_t score) noexcept;

	[[nodiscard]] const std::array<std::size_t, kRankingCapacity>& GetRanking() const noexcept
	{
		return ranking_;
	}
	[[nodiscard]] std::size_t GetRankingCount() const noexcept { return rankingCount_; }
	[[nodiscard]] std::optional<std::size_t> GetLastSubmittedRank() const noexcept {
		return lastSubmittedRank_;
	}
	[[nodiscard]] std::optional<std::size_t> GetLastSubmittedScore() const noexcept {
		return lastSubmittedScore_;
	}

private:
	GameFlowState() noexcept;
	void LoadRanking() noexcept;
	void SaveRanking() const noexcept;
	Audio* audio_ = nullptr;
	AudioClipHandle bgmClip_{};
	AudioVoiceHandle bgmVoice_{};
	float bgmVolume_ = 0.5f;
	float seVolume_ = 0.5f;
	std::array<std::size_t, kRankingCapacity> ranking_{};
	std::size_t rankingCount_ = 0;
	std::optional<std::size_t> lastSubmittedRank_{};
	std::optional<std::size_t> lastSubmittedScore_{};
};
