#pragma once

#include "audio/Audio.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <utility>

class GameFlowState final {
public:
	enum class BgmTrack : std::size_t {
		Title,
		Gameplay,
		Result,
		Count,
	};

	static constexpr std::size_t kRankingCapacity = 5;

	static GameFlowState& GetInstance() noexcept
	{
		static GameFlowState instance;
		return instance;
	}

	void EnsureBgm(Audio* audio, BgmTrack track)
	{
		if (!audio) { return; }
		audio_ = audio;
		audio_->SetSoundEffectVolume(seVolume_);
		const std::size_t trackIndex = static_cast<std::size_t>(track);
		if (trackIndex >= bgmClips_.size()) { return; }
		if (activeBgmTrack_ != track && bgmVoice_) {
			(void)audio_->StopVoice(bgmVoice_);
			bgmVoice_ = {};
		}
		activeBgmTrack_ = track;
		if (!bgmClips_[trackIndex]) {
			constexpr const char* paths[] = {
				"Resources/audio/bgm/maoudamashii_title_cyber34_loop.mp3",
				"Resources/bgm.wav",
				"Resources/audio/bgm/maoudamashii_result_jingle02.mp3",
			};
			bgmClips_[trackIndex] = audio_->LoadAudio(paths[trackIndex]);
		}
		if (bgmClips_[trackIndex] && !audio_->IsVoiceActive(bgmVoice_)) {
			Audio::PlaySettings settings{};
			settings.loop = track != BgmTrack::Result;
			settings.useSoundEffectVolume = false;
			settings.volume = bgmVolume_;
			bgmVoice_ = audio_->Play(bgmClips_[trackIndex], settings);
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

	void SetActiveStageSaveName(std::string saveName)
	{
		activeStageSaveName_ = std::move(saveName);
	}

	[[nodiscard]] const std::string& GetActiveStageSaveName() const noexcept
	{
		return activeStageSaveName_;
	}

private:
	GameFlowState() noexcept;
	void LoadRanking() noexcept;
	void SaveRanking() const noexcept;
	Audio* audio_ = nullptr;
	std::array<AudioClipHandle, static_cast<std::size_t>(BgmTrack::Count)> bgmClips_{};
	AudioVoiceHandle bgmVoice_{};
	BgmTrack activeBgmTrack_ = BgmTrack::Count;
	float bgmVolume_ = 0.5f;
	float seVolume_ = 0.5f;
	std::array<std::size_t, kRankingCapacity> ranking_{};
	std::size_t rankingCount_ = 0;
	std::optional<std::size_t> lastSubmittedRank_{};
	std::optional<std::size_t> lastSubmittedScore_{};
	std::string activeStageSaveName_;
};
