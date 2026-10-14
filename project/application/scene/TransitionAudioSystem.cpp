#include "scene/TransitionAudioSystem.h"
#include "base/Logger.h"
#include <algorithm>

namespace scene {
bool TransitionAudioSystem::Initialize(Audio* audio, const std::string& directory) {
    Finalize();
    if (!audio) return false;
    audio_ = audio;
    for (std::size_t i = 0; i < clips_.size(); ++i) {
        clips_[i] = audio_->LoadAudio(directory + "/" + kTransitionSoundFiles[i]);
        if (!clips_[i]) {
            Finalize();
            return false;
        }
    }
    return true;
}

void TransitionAudioSystem::PlayCues(TransitionSoundMask cues) {
    if (!audio_) return;
    for (auto& voice : voices_) if (voice && !audio_->IsVoiceActive(voice)) voice = {};
    Audio::PlaySettings settings;
    settings.volume = kTransitionSoundGain;
    for (std::size_t i = 0; i < clips_.size(); ++i) {
        if ((cues & (TransitionSoundMask{1} << i)) == 0) continue;
        const auto free = std::find(voices_.begin(), voices_.end(), AudioVoiceHandle{});
        if (free == voices_.end()) break; // Keep active cues intact rather than cutting them under overload.
        *free = audio_->Play(clips_[i], settings);
        if (!*free) Logger::Warning("Transition SE voice unavailable; continuing silently.");
    }
}

void TransitionAudioSystem::Finalize() {
    if (audio_) {
        // SourceVoice buffers borrow PCM; stop own voices before unloading any own clip.
        for (auto voice : voices_) if (voice && audio_->IsVoiceActive(voice)) (void)audio_->StopVoice(voice);
        for (auto clip : clips_) if (clip) (void)audio_->UnloadAudio(clip);
    }
    voices_ = {};
    clips_ = {};
    audio_ = nullptr;
}
}
