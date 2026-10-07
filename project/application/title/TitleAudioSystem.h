#pragma once

#include "audio/Audio.h"
#include "title/TitleAudioMix.h"

#include <array>
#include <string>

namespace title {

class TitleAudioSystem final {
public:
    TitleAudioSystem() = default;
    ~TitleAudioSystem() { Finalize(); }
    TitleAudioSystem(const TitleAudioSystem&) = delete;
    TitleAudioSystem& operator=(const TitleAudioSystem&) = delete;
    TitleAudioSystem(TitleAudioSystem&&) = delete;
    TitleAudioSystem& operator=(TitleAudioSystem&&) = delete;

    [[nodiscard]] bool Initialize(Audio* audio, const std::string& directory = "Resources/title/audio");
    void Update(const AudioMix& mix);
    void Finalize();

private:
    static constexpr std::size_t kVoiceCount = kAmbienceCount + 1;
    // Framework outlives Scene. Title-exclusive paths must not be shared with other systems.
    Audio* audio_ = nullptr;
    std::array<AudioClipHandle, kVoiceCount> clips_{};
    std::array<AudioVoiceHandle, kVoiceCount> voices_{};
    std::array<float, kVoiceCount> volumes_{};
    std::array<bool, kVoiceCount> paused_{};
};

} // namespace title
