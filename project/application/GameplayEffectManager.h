#pragma once

#include "base/DirectXCommon.h"
#include "effect/ParticleManager.h"
#include "math/Matrix.h"

#include <cstdint>
#include <vector>

class GameplayEffectManager {
public:
    struct ScreenPostEffectModifier {
        bool active = false;
        bool forceChainMode = false;
        bool forceHSVFilter = false;
        bool forceVignette = false;
        bool forceRadialBlur = false;
        bool forceRandomNoise = false;

        float hsvSaturationAdd = 0.0f;
        float hsvValueAdd = 0.0f;

        float vignetteScaleAdd = 0.0f;
        float vignettePowerAdd = 0.0f;
        float vignetteIntensityAdd = 0.0f;

        Vector2 radialCenter = { 0.5f, 0.5f };
        float radialBlurWidthAdd = 0.0f;
        float radialBlurIntensityAdd = 0.0f;
        int radialSampleCountMin = 0;

        float randomNoiseStrengthAdd = 0.0f;
        float randomNoiseScale = 1200.0f;
        bool randomNoiseAnimate = false;
        int randomNoiseMode = 1;
    };

    struct HarvestPopupSpriteState {
        bool visible = false;
        Vector2 position = { 0.0f, 0.0f };
        Vector2 size = { 0.0f, 0.0f };
        float alpha = 0.0f;
    };

    void PlayHarvestEffect(const Vector3& position, int32_t price);
    void PlayDigitalImpactEffect(const Vector3& worldPosition);
    void SetDemoMode(bool enabled);
    bool IsDemoMode() const { return demoMode_; }
    void ApplyRecordingDemoDefaults();
    void SetHarvestPopupDrawListEnabled(bool enabled) { drawHarvestPopupTextInDrawList_ = enabled; }
    void Update(float deltaTime);
    void DrawGameplayEffects(
        const Vector2& viewportTopLeft,
        const Vector2& viewportSize,
        const Matrix4x4* viewProjection = nullptr) const;
    bool DrawGameplayEffectImGui();

    ScreenPostEffectModifier GetScreenPostEffectModifier() const;
    Vector2 GetViewportShakeOffset() const;
    bool ConsumeHarvestParticleEmitSettings(GPUParticleEmitSettings& outSettings);
    bool ConsumeDigitalParticleEmitSettings(GPUParticleEmitSettings& outSettings);
    uint32_t GetHarvestBurstParticleCount() const;

    bool IsHarvestActive() const { return harvestActive_; }
    bool IsDigitalImpactActive() const { return digitalActive_; }
    Vector2 GetLastDigitalScreenPosition() const { return lastDigitalScreenPosition_; }
    Vector2 GetLastViewportMin() const { return lastViewportMin_; }
    Vector2 GetLastViewportSize() const { return lastViewportSize_; }
    bool WasLastOverlayDrawn() const { return lastOverlayDrawn_; }
    float GetHarvestTimer() const { return harvestTimer_; }
    float GetHarvestDuration() const { return harvestDuration_; }
    float GetHarvestPower() const;
    HarvestPopupSpriteState GetHarvestPopupSpriteState(const Vector2& textureSize) const;
    int32_t GetDebugHarvestPrice() const { return debugHarvestPrice_; }
    Vector3 GetDebugHarvestPosition() const { return debugHarvestPosition_; }
    Vector3 GetDebugDigitalImpactPosition() const { return debugDigitalPosition_; }

private:
    struct DigitalParticle {
        float angle = 0.0f;
        float speed = 0.0f;
        float startRadius = 0.0f;
        float size = 4.0f;
        float lifeTime = 0.5f;
        float spin = 0.0f;
        uint32_t colorIndex = 0;
    };

    struct DigitalRay {
        float angle = 0.0f;
        float lengthScale = 1.0f;
        float width = 1.0f;
        float delay = 0.0f;
    };

    float GetNormalizedHarvestTime() const;
    float GetNormalizedPopupTime() const;
    float GetRadialImpactPower() const;
    float GetNoiseImpactPower() const;
    float GetFlashPower() const;
    float GetNormalizedDigitalTime() const;
    float GetDigitalPower() const;
    float GetDigitalRadialImpactPower() const;
    float GetDigitalNoiseImpactPower() const;
    float GetDigitalFlashPower() const;

    bool harvestActive_ = false;
    bool pendingHarvestParticleEmit_ = false;
    bool enableScreenShake_ = true;
    bool enableParticles_ = true;
    bool demoMode_ = false;
    bool drawHarvestPopupTextInDrawList_ = true;

    float harvestTimer_ = 0.0f;
    float harvestDuration_ = 0.58f;
    float normalDuration_ = 0.58f;
    float demoDuration_ = 1.1f;
    float popupDuration_ = 1.0f;
    float flashPower_ = 0.85f;
    float popupScale_ = 1.45f;
    float effectPower_ = 1.0f;
    int particleCount_ = 256;
    Vector3 harvestPosition_ = { 0.0f, 1.5f, 8.0f };
    int32_t harvestPrice_ = 120;

    Vector3 debugHarvestPosition_ = { 0.0f, 1.5f, 8.0f };
    int32_t debugHarvestPrice_ = 120;

    bool digitalActive_ = false;
    bool pendingDigitalParticleEmit_ = false;
    bool enableDigitalParticles_ = true;
    bool enableDigitalRing_ = true;
    bool enableDigitalScreenPostEffect_ = true;
    bool showDigitalForceTestOverlay_ = false;
    float digitalTimer_ = 0.0f;
    float digitalDuration_ = 0.68f;
    float digitalNormalDuration_ = 0.68f;
    float digitalDemoDuration_ = 1.0f;
    float digitalEffectPower_ = 1.0f;
    float digitalRingScale_ = 1.0f;
    float digitalRingThickness_ = 6.0f;
    int digitalParticleCount_ = 180;
    float digitalFlashPower_ = 1.15f;
    float digitalRadialBlurPower_ = 1.2f;
    Vector3 digitalPosition_ = { 0.0f, 1.5f, 8.0f };
    Vector3 debugDigitalPosition_ = { 0.0f, 1.5f, 8.0f };
    std::vector<DigitalParticle> digitalParticles_;
    std::vector<DigitalRay> digitalRays_;
    mutable Vector2 lastDigitalScreenPosition_ = { 0.0f, 0.0f };
    mutable Vector2 lastViewportMin_ = { 0.0f, 0.0f };
    mutable Vector2 lastViewportSize_ = { 0.0f, 0.0f };
    mutable bool lastOverlayDrawn_ = false;
};
