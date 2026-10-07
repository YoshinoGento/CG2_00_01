#include "title/TitleAudioSystem.h"

#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <limits>
#include <source_location>
#include <thread>
#include <chrono>

namespace {
struct ComSession {
    HRESULT result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    ~ComSession() { if (SUCCEEDED(result)) CoUninitialize(); }
};
void Check(bool value, std::source_location location = std::source_location::current()) {
    if (value) return;
    std::cerr << "Title audio assertion at " << location.line() << '\n';
    throw std::runtime_error("Title audio assertion failed");
}
}

int main() {
    ComSession com;
    Check(SUCCEEDED(com.result));
    Audio backend;
    Check(backend.Initialize());
    title::TitleAudioSystem titleAudio;
    Check(!titleAudio.Initialize(nullptr));
    Check(!titleAudio.Initialize(&backend,"generated/codex_checks/title_ambience_20261005/missing"));
    Check(backend.GetStatistics().loadedClipCount == 0);
    const std::filesystem::path fixtures = "generated/codex_checks/title_ambience_20261005/fixtures";
    std::filesystem::create_directories(fixtures);
    std::filesystem::copy_file("project/Resources/title/audio/water.wav",fixtures/"unrelated.wav",
        std::filesystem::copy_options::overwrite_existing);
    std::filesystem::copy_file("project/Resources/title/audio/morning.wav",fixtures/"morning.wav",
        std::filesystem::copy_options::overwrite_existing);
    const auto unrelatedClip = backend.LoadAudio((fixtures/"unrelated.wav").string());
    Check(unrelatedClip.IsValid());
    Audio::PlaySettings paused;
    paused.loop = true; paused.startPaused = true; paused.volume = 0;
    const auto unrelatedVoice = backend.Play(unrelatedClip,paused);
    Check(unrelatedVoice.IsValid());
    const auto baseline = backend.GetStatistics();
    Check(!titleAudio.Initialize(&backend,fixtures.string()));
    Check(backend.GetStatistics().loadedClipCount == baseline.loadedClipCount);
    Check(backend.GetStatistics().activeVoiceCount == baseline.activeVoiceCount);
    constexpr const char* directory = "project/Resources/title/audio";
    for (int reentry=0; reentry<3; ++reentry) {
        Check(titleAudio.Initialize(&backend,directory));
        const auto loaded = backend.GetStatistics();
        Check(loaded.loadedClipCount == baseline.loadedClipCount+5);
        Check(loaded.activeVoiceCount == baseline.activeVoiceCount+5);
        constexpr std::size_t expectedPcm = 48658648;
        Check(loaded.decodedPcmBytes == baseline.decodedPcmBytes+expectedPcm);
        for (int i=0; i<1600; ++i) {
            auto mix = title::MakeAudioMix(i*.025f,0);
            // Exercise real voices quietly; hearing evaluation belongs to the game runtime.
            for (auto& gain : mix.ambience) gain *= .01f;
            mix.water *= .01f;
            titleAudio.Update(mix);
            backend.Update(.025f);
            Check(backend.GetStatistics().activeVoiceCount == loaded.activeVoiceCount);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        title::AudioMix invalid;
        invalid.ambience.fill(std::numeric_limits<float>::quiet_NaN());
        invalid.water = std::numeric_limits<float>::infinity();
        titleAudio.Update(invalid);
        titleAudio.Update(title::MakeAudioMix(0,1));
        titleAudio.Finalize(); titleAudio.Finalize();
        const auto final = backend.GetStatistics();
        Check(final.loadedClipCount == baseline.loadedClipCount);
        Check(final.activeVoiceCount == baseline.activeVoiceCount);
        Check(final.decodedPcmBytes == baseline.decodedPcmBytes);
        Check(backend.IsVoiceActive(unrelatedVoice));
        std::cout << "reentry " << reentry << ": title PCM " << loaded.decodedPcmBytes-baseline.decodedPcmBytes
                  << ", cleanup preserves unrelated voice PASS\n";
    }
    Check(backend.StopVoice(unrelatedVoice));
    Check(backend.UnloadAudio(unrelatedClip));
    Check(titleAudio.Initialize(&backend,directory));
    backend.StopAllVoices();
    titleAudio.Update(title::MakeAudioMix(0,0));
    Check(backend.GetStatistics().activeVoiceCount == 0);
    Check(backend.GetStatistics().loadedClipCount == 0);
    Check(backend.GetStatistics().decodedPcmBytes == 0);
    backend.Finalize();
    std::cout << "Title audio: actual XAudio2/MF load, partial failure, wrap updates, reentry, ownership PASS\n";
}
