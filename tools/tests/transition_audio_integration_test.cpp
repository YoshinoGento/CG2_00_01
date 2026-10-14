#include "scene/TransitionAudioSystem.h"
#include <chrono>
#include <filesystem>
#include <iostream>
#include <source_location>
#include <stdexcept>
#include <thread>

namespace {
struct ComSession {
    HRESULT result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    ~ComSession() { if (SUCCEEDED(result)) CoUninitialize(); }
};
void Check(bool value, std::source_location at = std::source_location::current()) {
    if (!value) throw std::runtime_error("transition audio assertion at line " + std::to_string(at.line()));
}
void WaitForAudio(Audio& backend) {
    for (int i=0; i<100; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        backend.Update(.01f);
    }
}
}
int main() {
    ComSession com;
    Check(SUCCEEDED(com.result));
    Audio backend;
    Check(backend.Initialize());
    scene::TransitionAudioSystem effects;
    Check(!effects.Initialize(nullptr));
    effects.PlayCues(~scene::TransitionSoundMask{0});
    constexpr const char* directory = "project/Resources/transition/audio";
    const auto unrelatedClip = backend.LoadAudio(std::string(directory) + "/ledger.wav");
    Check(static_cast<bool>(unrelatedClip));
    Audio::PlaySettings paused;
    paused.loop = true; paused.startPaused = true; paused.volume = 0;
    const auto unrelatedVoice = backend.Play(unrelatedClip,paused);
    Check(static_cast<bool>(unrelatedVoice));
    const auto baseline = backend.GetStatistics();
    const std::filesystem::path partial = "generated/codex_checks/transition_audio_partial";
    std::filesystem::create_directories(partial);
    std::filesystem::copy_file(std::string(directory)+"/water_drop.wav",partial/"water_drop.wav",
        std::filesystem::copy_options::overwrite_existing);
    Check(!effects.Initialize(&backend,partial.string()));
    Check(backend.GetStatistics().loadedClipCount == baseline.loadedClipCount);
    Check(backend.GetStatistics().decodedPcmBytes == baseline.decodedPcmBytes);
    // Unrelated clip uses a different canonical path: Audio cache does not reference-count owners.
    const std::filesystem::path own = "generated/codex_checks/transition_audio_full";
    std::filesystem::create_directories(own);
    for (const char* name : scene::kTransitionSoundFiles)
        std::filesystem::copy_file(std::filesystem::path(directory)/name,own/name,
            std::filesystem::copy_options::overwrite_existing);
    constexpr std::size_t expectedPcm = 276480 * 2; // Backend retains forward and reversed PCM.
    for (int entry=0; entry<3; ++entry) {
        Check(effects.Initialize(&backend,own.string()));
        Check(backend.GetStatistics().loadedClipCount == baseline.loadedClipCount + scene::kTransitionSoundCount);
        Check(backend.GetStatistics().decodedPcmBytes == baseline.decodedPcmBytes + expectedPcm);
        effects.PlayCues(scene::TransitionSoundMask{1} << 31);
        Check(backend.GetStatistics().activeVoiceCount == baseline.activeVoiceCount);
        effects.PlayCues(~scene::TransitionSoundMask{0});
        Check(backend.GetStatistics().activeVoiceCount == baseline.activeVoiceCount + effects.kMaximumVoices);
        for (int repeat=0; repeat<100; ++repeat) effects.PlayCues(~scene::TransitionSoundMask{0});
        Check(backend.GetStatistics().activeVoiceCount == baseline.activeVoiceCount + effects.kMaximumVoices);
        WaitForAudio(backend);
        Check(backend.GetStatistics().activeVoiceCount == baseline.activeVoiceCount);
        for (std::size_t i=0; i<scene::kTransitionSoundCount; ++i) {
            effects.PlayCues(scene::TransitionSoundMask{1} << i);
            Check(backend.GetStatistics().activeVoiceCount == baseline.activeVoiceCount + 1);
            WaitForAudio(backend);
            Check(backend.GetStatistics().activeVoiceCount == baseline.activeVoiceCount);
        }
        effects.PlayCues(scene::SoundBit(scene::TransitionSoundCue::WaterReveal));
        effects.Finalize(); effects.Finalize();
        Check(backend.GetStatistics().loadedClipCount == baseline.loadedClipCount);
        Check(backend.GetStatistics().decodedPcmBytes == baseline.decodedPcmBytes);
        Check(backend.GetStatistics().activeVoiceCount == baseline.activeVoiceCount);
        Check(backend.IsVoiceActive(unrelatedVoice));
    }
    Check(backend.StopVoice(unrelatedVoice));
    Check(backend.UnloadAudio(unrelatedClip));
    backend.Finalize();
    std::cout << "Transition XAudio2/MF: null/partial failure, seven clips, voice cap, completion/reentry/ownership PASS\n";
}
