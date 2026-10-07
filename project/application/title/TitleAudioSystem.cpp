#include "title/TitleAudioSystem.h"
#include "base/Logger.h"

namespace title {

bool TitleAudioSystem::Initialize(Audio* audio, const std::string& directory) {
    Finalize();
    if (!audio) return false;
    audio_ = audio;
    constexpr std::array<const char*, kVoiceCount> names{
        "morning.wav", "day.wav", "evening.wav", "night.wav", "water.wav"};
    Audio::PlaySettings settings;
    settings.loop = true;
    settings.startPaused = true;
    settings.volume = 0;
    for (std::size_t i=0; i<kVoiceCount; ++i) {
        clips_[i] = audio_->LoadAudio(directory + "/" + names[i]);
        if (clips_[i]) voices_[i] = audio_->Play(clips_[i], settings);
        if (!voices_[i]) {
            Finalize();
            return false;
        }
        paused_[i] = true;
    }
    Logger::Info("Title ambience ready: five retained loop voices.");
    return true;
}

void TitleAudioSystem::Update(const AudioMix& mix) {
    if (!audio_) return;
    const auto safe = SanitizeAudioMix(mix);
    for (std::size_t i=0; i<kVoiceCount; ++i) {
        const float gain = i<kAmbienceCount ? safe.ambience[i] : safe.water;
        if (gain != volumes_[i]) {
            if (!audio_->SetVoiceVolume(voices_[i], gain)) {
                Logger::Warning("Title ambience stopped: voice volume update failed.");
                Finalize(); return;
            }
            volumes_[i] = gain;
        }
        const bool pause = gain == 0;
        if (pause != paused_[i]) {
            const bool success = pause ? audio_->PauseVoice(voices_[i]) : audio_->ResumeVoice(voices_[i]);
            if (!success) {
                Logger::Warning("Title ambience stopped: voice state update failed.");
                Finalize(); return;
            }
            paused_[i] = pause;
        }
    }
}

void TitleAudioSystem::Finalize() {
    if (audio_) {
        // Voices retain PCM pointers. Stop every own voice before releasing any clip.
        bool success = true;
        for (auto voice : voices_) if (voice) success = audio_->StopVoice(voice) && success;
        for (auto clip : clips_) if (clip) success = audio_->UnloadAudio(clip) && success;
        if (success) Logger::Info("Title ambience released.");
        else Logger::Warning("Title ambience cleanup encountered an invalidated handle.");
    }
    audio_ = nullptr;
    clips_ = {};
    voices_ = {};
    volumes_ = {};
    paused_ = {};
}

} // namespace title
