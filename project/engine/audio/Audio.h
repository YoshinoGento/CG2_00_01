#pragma once

#include <xaudio2.h>
#include <wrl.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <string>
#include <thread>
#include <vector>

struct AudioClipHandle final {
	static constexpr uint32_t kInvalidValue = 0;

	uint32_t value = kInvalidValue;

	[[nodiscard]] constexpr bool IsValid() const noexcept { return value != kInvalidValue; }
	explicit constexpr operator bool() const noexcept { return IsValid(); }

	friend constexpr bool operator==(AudioClipHandle lhs, AudioClipHandle rhs) noexcept = default;
};

struct AudioVoiceHandle final {
	static constexpr uint32_t kInvalidValue = 0;

	uint32_t value = kInvalidValue;

	[[nodiscard]] constexpr bool IsValid() const noexcept { return value != kInvalidValue; }
	explicit constexpr operator bool() const noexcept { return IsValid(); }

	friend constexpr bool operator==(AudioVoiceHandle lhs, AudioVoiceHandle rhs) noexcept = default;
};

enum class AudioPlaybackDirection : uint8_t {
	Forward,
	Reverse,
};

class Audio final {
public:
	struct PlaySettings final {
		bool loop = false;
		bool startPaused = false;
		float volume = 1.0f;
		float playbackRate = 1.0f;
		AudioPlaybackDirection direction = AudioPlaybackDirection::Forward;
	};

	struct Statistics final {
		std::size_t loadedClipCount = 0;
		std::size_t activeVoiceCount = 0;
		std::size_t decodedPcmBytes = 0;
		float temporalPlaybackRate = 1.0f;
		float targetTemporalPlaybackRate = 1.0f;
		AudioPlaybackDirection temporalDirection = AudioPlaybackDirection::Forward;
	};

	Audio() = default;
	~Audio();
	Audio(const Audio&) = delete;
	Audio& operator=(const Audio&) = delete;
	Audio(Audio&&) = delete;
	Audio& operator=(Audio&&) = delete;

	[[nodiscard]] bool Initialize();
	void Finalize();
	void Update(float realDeltaSeconds);

	[[nodiscard]] AudioClipHandle LoadAudio(const std::string& filename);
	[[nodiscard]] bool UnloadAudio(AudioClipHandle clipHandle);
	void UnloadAllAudio();

	[[nodiscard]] AudioVoiceHandle PlayWave(AudioClipHandle clipHandle, bool loop = false);
	[[nodiscard]] AudioVoiceHandle Play(AudioClipHandle clipHandle, const PlaySettings& settings);
	[[nodiscard]] bool StopVoice(AudioVoiceHandle voiceHandle);
	[[nodiscard]] bool PauseVoice(AudioVoiceHandle voiceHandle);
	[[nodiscard]] bool ResumeVoice(AudioVoiceHandle voiceHandle);
	[[nodiscard]] bool SetVoiceVolume(AudioVoiceHandle voiceHandle, float volume);
	[[nodiscard]] bool SetVoicePlaybackRate(
		AudioVoiceHandle voiceHandle,
		float playbackRate,
		float transitionSeconds = 0.0f);
	[[nodiscard]] bool SetVoiceDirection(
		AudioVoiceHandle voiceHandle,
		AudioPlaybackDirection direction);
	[[nodiscard]] bool IsVoiceActive(AudioVoiceHandle voiceHandle) const;
	void StopAllVoices();

	// Temporal settings affect active voices and voices created after this call.
	void SetGlobalTemporalState(
		AudioPlaybackDirection direction,
		float playbackRate,
		float transitionSeconds = 0.0f);
	[[nodiscard]] Statistics GetStatistics() const;

private:
	struct Clip final {
		std::wstring canonicalPath;
		WAVEFORMATEXTENSIBLE waveFormat{};
		std::vector<uint8_t> forwardPcm;
		std::vector<uint8_t> reversePcm;
		uint32_t frameCount = 0;
	};

	struct Voice final {
		AudioClipHandle clipHandle{};
		IXAudio2SourceVoice* sourceVoice = nullptr;
		AudioPlaybackDirection baseDirection = AudioPlaybackDirection::Forward;
		AudioPlaybackDirection appliedDirection = AudioPlaybackDirection::Forward;
		bool loop = false;
		bool paused = false;
		float volume = 1.0f;
		float currentBasePlaybackRate = 1.0f;
		float targetBasePlaybackRate = 1.0f;
		float baseRateRampStart = 1.0f;
		float baseRateRampDuration = 0.0f;
		float baseRateRampElapsed = 0.0f;
		uint32_t submittedDirectionFrame = 0;
		uint64_t samplesPlayedAtSubmit = 0;
	};

	[[nodiscard]] bool IsOwnerThread(const char* operation) const;
	[[nodiscard]] bool DecodeAudioFile(const std::wstring& filename, Clip& outputClip) const;
	[[nodiscard]] std::wstring ResolveCanonicalPath(const std::string& filename) const;
	[[nodiscard]] AudioClipHandle AllocateClipHandle();
	[[nodiscard]] AudioVoiceHandle AllocateVoiceHandle();
	[[nodiscard]] Voice* FindVoice(AudioVoiceHandle voiceHandle);
	[[nodiscard]] const Voice* FindVoice(AudioVoiceHandle voiceHandle) const;
	[[nodiscard]] AudioPlaybackDirection ResolveAppliedDirection(const Voice& voice) const;
	[[nodiscard]] uint32_t GetCurrentOriginalFrame(const Voice& voice, const Clip& clip) const;
	[[nodiscard]] bool ResubmitVoice(
		Voice& voice,
		const Clip& clip,
		AudioPlaybackDirection direction,
		uint32_t originalFrame);
	[[nodiscard]] bool ApplyVoiceFrequencyRatio(Voice& voice) const;
	void DestroyVoice(Voice& voice) noexcept;
	void UpdateTemporalRate(float realDeltaSeconds);
	void UpdateBaseRate(Voice& voice, float realDeltaSeconds) const;

	Microsoft::WRL::ComPtr<IXAudio2> xAudio2_;
	IXAudio2MasteringVoice* masterVoice_ = nullptr;
	std::map<uint32_t, Clip> clips_;
	std::map<uint32_t, Voice> voices_;
	std::map<std::wstring, AudioClipHandle> clipCache_;
	uint32_t nextClipId_ = 1;
	uint32_t nextVoiceId_ = 1;
	AudioPlaybackDirection temporalDirection_ = AudioPlaybackDirection::Forward;
	float currentTemporalPlaybackRate_ = 1.0f;
	float targetTemporalPlaybackRate_ = 1.0f;
	float temporalRateRampStart_ = 1.0f;
	float temporalRateRampDuration_ = 0.0f;
	float temporalRateRampElapsed_ = 0.0f;
	std::thread::id ownerThread_{};
	bool mediaFoundationStarted_ = false;
	bool initialized_ = false;
};
