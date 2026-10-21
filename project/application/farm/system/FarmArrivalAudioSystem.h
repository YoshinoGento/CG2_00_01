#pragma once
#include "audio/Audio.h"
#include "farm/system/FarmStartPresentationSystem.h"
#include <array>

class FarmArrivalAudioSystem final {
public:
    ~FarmArrivalAudioSystem() { Finalize(); }
    bool Initialize(Audio* audio);
    void Update(const FarmStartPresentationSystem::Frame& frame, bool playing, bool focused);
    void Finalize();
private:
    Audio* audio_ = nullptr; // Framework lifetime; private clip paths avoid shared-cache invalidation.
    std::array<AudioClipHandle,2> clips_{};
    AudioVoiceHandle ambience_{}, impact_{};
    float environmentGain_=.4f, waterGain_=.4f, previousTime_=-1;
    bool impactPlayed_=false, begun_=false;
};
