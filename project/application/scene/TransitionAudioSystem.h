#pragma once
#include "audio/Audio.h"
#include "scene/TransitionSound.h"
#include <array>
#include <string>

namespace scene {
class TransitionAudioSystem final {
public:
    static constexpr std::size_t kMaximumVoices = 4;
    TransitionAudioSystem() = default;
    ~TransitionAudioSystem() { Finalize(); }
    TransitionAudioSystem(const TransitionAudioSystem&) = delete;
    TransitionAudioSystem& operator=(const TransitionAudioSystem&) = delete;
    TransitionAudioSystem(TransitionAudioSystem&&) = delete;
    TransitionAudioSystem& operator=(TransitionAudioSystem&&) = delete;
    [[nodiscard]] bool Initialize(Audio* audio, const std::string& directory = "Resources/transition/audio");
    void PlayCues(TransitionSoundMask cues);
    void Finalize();
private:
    // Framework Audio outlives this owner; these cached paths must remain exclusive.
    Audio* audio_ = nullptr;
    std::array<AudioClipHandle, kTransitionSoundCount> clips_{};
    std::array<AudioVoiceHandle, kMaximumVoices> voices_{};
};
}
