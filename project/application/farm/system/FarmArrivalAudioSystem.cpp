#include "farm/system/FarmArrivalAudioSystem.h"
#include "title/TitleAudioSettingsSystem.h"

bool FarmArrivalAudioSystem::Initialize(Audio* audio) {
    Finalize();
    if(!audio) return false;
    audio_=audio;
    clips_[0]=audio_->LoadAudio("Resources/farm/arrival/morning.wav");
    clips_[1]=audio_->LoadAudio("Resources/farm/arrival/dew.wav");
    if(!clips_[0] || !clips_[1]) { Finalize(); return false; }
    return true;
}
void FarmArrivalAudioSystem::Update(const FarmStartPresentationSystem::Frame& frame, bool playing, bool focused) {
    if(!audio_) return;
    if(!playing || !frame.arrival) {
        if(ambience_) (void)audio_->StopVoice(ambience_);
        if(impact_ && audio_->IsVoiceActive(impact_)) (void)audio_->StopVoice(impact_);
        ambience_={}; impact_={}; begun_=impactPlayed_=false; previousTime_=-1;
        return;
    }
    if(!begun_) {
        title::TitleAudioSettingsSystem settings;
        settings.Initialize(); // Read only: honor current sliders without writing settings.
        environmentGain_=settings.GetFrame().volume[0]*.01f;
        waterGain_=settings.GetFrame().volume[1]*.01f;
        Audio::PlaySettings play; play.volume=0; play.loop=true;
        ambience_=audio_->Play(clips_[0],play);
        begun_=true;
    }
    const auto arrival=farm::arrival::Sample(frame.arrivalTime,{});
    if(ambience_) {
        (void)audio_->SetVoiceVolume(ambience_, focused ? environmentGain_*.30f*arrival.ambience*frame.bars : 0);
    }
    if(!impactPlayed_ && previousTime_<farm::arrival::kImpact && frame.arrivalTime>=farm::arrival::kImpact) {
        Audio::PlaySettings play; play.volume=focused ? waterGain_*.55f : 0;
        if(play.volume>0) impact_=audio_->Play(clips_[1],play);
        impactPlayed_=true;
    }
    if(!focused && impact_ && audio_->IsVoiceActive(impact_)) (void)audio_->SetVoiceVolume(impact_,0);
    previousTime_=frame.arrivalTime;
}
void FarmArrivalAudioSystem::Finalize() {
    if(audio_) {
        if(ambience_) (void)audio_->StopVoice(ambience_);
        if(impact_ && audio_->IsVoiceActive(impact_)) (void)audio_->StopVoice(impact_);
        for(auto clip:clips_) if(clip) (void)audio_->UnloadAudio(clip);
    }
    audio_=nullptr; clips_={}; ambience_={}; impact_={}; begun_=false;
}
