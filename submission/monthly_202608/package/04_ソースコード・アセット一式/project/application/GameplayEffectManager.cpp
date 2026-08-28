#include "GameplayEffectManager.h"

#include "base/ImGuiManager.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <random>

namespace {
constexpr float kPi = 3.14159265358979323846f;

float Clamp01(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

float Squared(float value) {
    return value * value;
}

Vector2 ProjectWorldToViewport(
    const Vector3& worldPosition,
    const Matrix4x4* viewProjection,
    const Vector2& viewportTopLeft,
    const Vector2& viewportSize) {
    const Vector2 fallback{
        viewportTopLeft.x + viewportSize.x * 0.5f,
        viewportTopLeft.y + viewportSize.y * 0.5f,
    };
    if (!viewProjection) {
        return fallback;
    }

    Vector3 ndc{};
    ndc.x =
        worldPosition.x * viewProjection->m[0][0] +
        worldPosition.y * viewProjection->m[1][0] +
        worldPosition.z * viewProjection->m[2][0] +
        viewProjection->m[3][0];
    ndc.y =
        worldPosition.x * viewProjection->m[0][1] +
        worldPosition.y * viewProjection->m[1][1] +
        worldPosition.z * viewProjection->m[2][1] +
        viewProjection->m[3][1];
    ndc.z =
        worldPosition.x * viewProjection->m[0][2] +
        worldPosition.y * viewProjection->m[1][2] +
        worldPosition.z * viewProjection->m[2][2] +
        viewProjection->m[3][2];
    const float w =
        worldPosition.x * viewProjection->m[0][3] +
        worldPosition.y * viewProjection->m[1][3] +
        worldPosition.z * viewProjection->m[2][3] +
        viewProjection->m[3][3];
    if (!std::isfinite(w) || w <= 0.000001f) {
        return fallback;
    }
    ndc.x /= w;
    ndc.y /= w;
    ndc.z /= w;
    if (!std::isfinite(ndc.x) || !std::isfinite(ndc.y) || !std::isfinite(ndc.z) ||
        ndc.z < 0.0f || ndc.z > 1.0f ||
        ndc.x < -1.25f || ndc.x > 1.25f ||
        ndc.y < -1.25f || ndc.y > 1.25f) {
        return fallback;
    }

    return {
        viewportTopLeft.x + (ndc.x + 1.0f) * 0.5f * viewportSize.x,
        viewportTopLeft.y + (1.0f - ndc.y) * 0.5f * viewportSize.y,
    };
}

#ifdef USE_IMGUI
ImVec2 Add(const ImVec2& a, const ImVec2& b) {
    return { a.x + b.x, a.y + b.y };
}

ImVec2 Mul(const ImVec2& v, float scale) {
    return { v.x * scale, v.y * scale };
}

ImU32 ColorWithAlpha(int r, int g, int b, float alpha) {
    return IM_COL32(r, g, b, static_cast<int>(std::clamp(alpha, 0.0f, 255.0f)));
}
#endif
}

void GameplayEffectManager::PlayHarvestEffect(const Vector3& position, int32_t price) {
    harvestActive_ = true;
    harvestTimer_ = 0.0f;
    harvestDuration_ = demoMode_ ? demoDuration_ : normalDuration_;
    harvestPosition_ = position;
    harvestPrice_ = (std::max)(price, 0);
    pendingHarvestParticleEmit_ = enableParticles_;
}

void GameplayEffectManager::PlayDigitalImpactEffect(const Vector3& worldPosition) {
    digitalActive_ = true;
    digitalTimer_ = 0.0f;
    digitalDuration_ = demoMode_ ? digitalDemoDuration_ : digitalNormalDuration_;
    digitalPosition_ = worldPosition;
    pendingDigitalParticleEmit_ = enableDigitalParticles_;

    digitalParticles_.clear();
    digitalRays_.clear();

    std::random_device seedGenerator;
    std::mt19937 engine(seedGenerator());
    std::uniform_real_distribution<float> angleDist(0.0f, kPi * 2.0f);
    std::uniform_real_distribution<float> speedDist(0.58f, 1.45f);
    std::uniform_real_distribution<float> radiusDist(0.00f, 0.06f);
    std::uniform_real_distribution<float> sizeDist(6.0f, 15.0f);
    std::uniform_real_distribution<float> lifeDist(0.36f, 0.90f);
    std::uniform_real_distribution<float> spinDist(-10.0f, 10.0f);

    const int particleBoostedCount = demoMode_
        ? static_cast<int>(digitalParticleCount_ * 1.60f)
        : digitalParticleCount_;
    const uint32_t safeParticleCount = static_cast<uint32_t>(
        std::clamp(particleBoostedCount, 0, 512));
    digitalParticles_.reserve(safeParticleCount);
    for (uint32_t i = 0; i < safeParticleCount; ++i) {
        DigitalParticle particle{};
        particle.angle = angleDist(engine);
        particle.speed = speedDist(engine);
        particle.startRadius = radiusDist(engine);
        particle.size = sizeDist(engine);
        particle.lifeTime = lifeDist(engine);
        particle.spin = spinDist(engine);
        particle.colorIndex = i % 3;
        digitalParticles_.push_back(particle);
    }

    constexpr uint32_t kRayCount = 44;
    std::uniform_real_distribution<float> lengthDist(0.80f, 1.55f);
    std::uniform_real_distribution<float> widthDist(0.8f, 2.4f);
    std::uniform_real_distribution<float> delayDist(0.0f, 0.035f);
    digitalRays_.reserve(kRayCount);
    for (uint32_t i = 0; i < kRayCount; ++i) {
        DigitalRay ray{};
        ray.angle = (static_cast<float>(i) / static_cast<float>(kRayCount)) * kPi * 2.0f;
        ray.angle += ((i % 2) == 0 ? 0.018f : -0.024f);
        ray.lengthScale = lengthDist(engine);
        ray.width = widthDist(engine);
        ray.delay = delayDist(engine);
        digitalRays_.push_back(ray);
    }
}

void GameplayEffectManager::SetDemoMode(bool enabled) {
    demoMode_ = enabled;
    if (demoMode_) {
        showDigitalForceTestOverlay_ = false;
    }
    harvestDuration_ = demoMode_ ? demoDuration_ : normalDuration_;
    digitalDuration_ = demoMode_ ? digitalDemoDuration_ : digitalNormalDuration_;
}

void GameplayEffectManager::ApplyRecordingDemoDefaults() {
    demoMode_ = true;
    enableScreenShake_ = true;
    enableParticles_ = true;

    normalDuration_ = 0.68f;
    demoDuration_ = 1.18f;
    popupDuration_ = 1.15f;
    flashPower_ = 0.95f;
    popupScale_ = 1.85f;
    effectPower_ = 1.35f;
    particleCount_ = 420;

    enableDigitalParticles_ = true;
    enableDigitalRing_ = true;
    enableDigitalScreenPostEffect_ = true;
    showDigitalForceTestOverlay_ = false;
    digitalNormalDuration_ = 0.72f;
    digitalDemoDuration_ = 1.0f;
    digitalEffectPower_ = 1.45f;
    digitalRingScale_ = 1.20f;
    digitalRingThickness_ = 7.5f;
    digitalParticleCount_ = 280;
    digitalFlashPower_ = 1.30f;
    digitalRadialBlurPower_ = 1.45f;

    harvestDuration_ = demoDuration_;
    digitalDuration_ = digitalDemoDuration_;
}

void GameplayEffectManager::Update(float deltaTime) {
    if (!harvestActive_ && !digitalActive_) {
        return;
    }

    if (!std::isfinite(deltaTime) || deltaTime <= 0.0f) {
        return;
    }

    const float safeDeltaTime = std::clamp(deltaTime, 1.0f / 240.0f, 1.0f / 15.0f);
    if (harvestActive_) {
        harvestTimer_ += safeDeltaTime;
        const float activeDuration = (std::max)(harvestDuration_, popupDuration_);
        if (harvestTimer_ >= activeDuration) {
            harvestTimer_ = activeDuration;
            harvestActive_ = false;
        }
    }
    if (digitalActive_) {
        digitalTimer_ += safeDeltaTime;
        if (digitalTimer_ >= digitalDuration_) {
            digitalTimer_ = digitalDuration_;
            digitalActive_ = false;
        }
    }
}

float GameplayEffectManager::GetNormalizedHarvestTime() const {
    if (harvestDuration_ <= 0.0f) {
        return 1.0f;
    }
    return Clamp01(harvestTimer_ / harvestDuration_);
}

float GameplayEffectManager::GetNormalizedPopupTime() const {
    if (popupDuration_ <= 0.0f) {
        return 1.0f;
    }
    return Clamp01(harvestTimer_ / popupDuration_);
}

float GameplayEffectManager::GetHarvestPower() const {
    if (!harvestActive_) {
        return 0.0f;
    }
    const float fade = 1.0f - GetNormalizedHarvestTime();
    return Squared(fade);
}

float GameplayEffectManager::GetRadialImpactPower() const {
    if (!harvestActive_) {
        return 0.0f;
    }
    const float fade = 1.0f - Clamp01(harvestTimer_ / (demoMode_ ? 0.28f : 0.22f));
    return Squared(fade);
}

float GameplayEffectManager::GetNoiseImpactPower() const {
    if (!harvestActive_) {
        return 0.0f;
    }
    return 1.0f - Clamp01(harvestTimer_ / 0.16f);
}

float GameplayEffectManager::GetNormalizedDigitalTime() const {
    if (digitalDuration_ <= 0.0f) {
        return 1.0f;
    }
    return Clamp01(digitalTimer_ / digitalDuration_);
}

float GameplayEffectManager::GetDigitalPower() const {
    if (!digitalActive_) {
        return 0.0f;
    }
    const float fade = 1.0f - GetNormalizedDigitalTime();
    return Squared(fade);
}

float GameplayEffectManager::GetDigitalRadialImpactPower() const {
    if (!digitalActive_) {
        return 0.0f;
    }
    const float fade = 1.0f - Clamp01(digitalTimer_ / (demoMode_ ? 0.30f : 0.22f));
    return Squared(fade);
}

float GameplayEffectManager::GetDigitalNoiseImpactPower() const {
    if (!digitalActive_) {
        return 0.0f;
    }
    return 1.0f - Clamp01(digitalTimer_ / 0.18f);
}

float GameplayEffectManager::GetDigitalFlashPower() const {
    if (!digitalActive_) {
        return 0.0f;
    }
    const float fade = 1.0f - Clamp01(digitalTimer_ / 0.13f);
    return digitalFlashPower_ * digitalEffectPower_ * Squared(fade);
}

float GameplayEffectManager::GetFlashPower() const {
    if (!harvestActive_) {
        return 0.0f;
    }
    const float fade = 1.0f - Clamp01(harvestTimer_ / 0.14f);
    const float demoFlashBoost = demoMode_ ? 1.15f : 1.0f;
    return flashPower_ * effectPower_ * demoFlashBoost * Squared(fade);
}

GameplayEffectManager::HarvestPopupSpriteState GameplayEffectManager::GetHarvestPopupSpriteState(const Vector2& textureSize) const {
    HarvestPopupSpriteState state{};
    if (!harvestActive_ || popupDuration_ <= 0.0f || textureSize.x <= 0.0f || textureSize.y <= 0.0f) {
        return state;
    }

    const float popupT = GetNormalizedPopupTime();
    const float alpha = Clamp01(1.0f - popupT);
    if (alpha <= 0.0f) {
        return state;
    }

    const float demoPopupBoost = demoMode_ ? 1.15f : 1.0f;
    const float impactScale = 1.0f + std::sin(Clamp01(harvestTimer_ / 0.20f) * kPi) * 0.45f;
    const float targetHeight = 92.0f * popupScale_ * demoPopupBoost * impactScale;
    const float aspect = textureSize.x / textureSize.y;

    state.visible = true;
    state.position = { 1280.0f * 0.5f, 720.0f * 0.42f - 108.0f * popupT };
    state.size = { targetHeight * aspect, targetHeight };
    state.alpha = alpha;
    return state;
}

GameplayEffectManager::ScreenPostEffectModifier GameplayEffectManager::GetScreenPostEffectModifier() const {
    ScreenPostEffectModifier modifier{};
    const float power = GetHarvestPower();
    const float radialPower = GetRadialImpactPower();
    const float noisePower = GetNoiseImpactPower();
    if (power > 0.0f || radialPower > 0.0f || noisePower > 0.0f) {
        modifier.active = true;
        modifier.forceChainMode = true;
        modifier.forceHSVFilter = modifier.forceHSVFilter || power > 0.0f;
        modifier.forceVignette = modifier.forceVignette || power > 0.0f;
        modifier.forceRadialBlur = modifier.forceRadialBlur || radialPower > 0.0f;
        modifier.forceRandomNoise = modifier.forceRandomNoise || noisePower > 0.0f;

        const float demoBoost = (demoMode_ ? 1.55f : 1.0f) * effectPower_;
        modifier.hsvSaturationAdd += 0.38f * power * demoBoost;
        modifier.hsvValueAdd += 0.18f * power * demoBoost;

        modifier.vignetteScaleAdd += 12.0f * power * demoBoost;
        modifier.vignettePowerAdd += 0.50f * power;
        modifier.vignetteIntensityAdd += 0.64f * power * demoBoost;

        modifier.radialCenter = { 0.5f, 0.5f };
        modifier.radialBlurWidthAdd += 0.070f * radialPower * demoBoost;
        modifier.radialBlurIntensityAdd += 0.95f * radialPower * demoBoost;
        modifier.radialSampleCountMin = (std::max)(
            modifier.radialSampleCountMin,
            radialPower > 0.0f ? 22 : 0);

        modifier.randomNoiseStrengthAdd += 0.12f * noisePower;
        modifier.randomNoiseScale = 1200.0f;
        modifier.randomNoiseAnimate = modifier.randomNoiseAnimate || noisePower > 0.0f;
        modifier.randomNoiseMode = 1;
    }

    if (enableDigitalScreenPostEffect_) {
        const float digitalPower = GetDigitalPower();
        const float digitalRadialPower = GetDigitalRadialImpactPower();
        const float digitalNoisePower = GetDigitalNoiseImpactPower();
        if (digitalPower > 0.0f || digitalRadialPower > 0.0f || digitalNoisePower > 0.0f) {
            modifier.active = true;
            modifier.forceChainMode = true;
            modifier.forceHSVFilter = modifier.forceHSVFilter || digitalPower > 0.0f;
            modifier.forceVignette = modifier.forceVignette || digitalPower > 0.0f;
            modifier.forceRadialBlur = modifier.forceRadialBlur || digitalRadialPower > 0.0f;
            modifier.forceRandomNoise = modifier.forceRandomNoise || digitalNoisePower > 0.0f;

            const float demoBoost = (demoMode_ ? 1.25f : 1.0f) * digitalEffectPower_;
            modifier.hsvSaturationAdd += 0.22f * digitalPower * demoBoost;
            modifier.hsvValueAdd += 0.14f * digitalPower * demoBoost;

            modifier.vignetteScaleAdd += 9.0f * digitalPower * demoBoost;
            modifier.vignettePowerAdd += 0.24f * digitalPower;
            modifier.vignetteIntensityAdd += 0.34f * digitalPower * demoBoost;

            modifier.radialCenter = { 0.5f, 0.5f };
            modifier.radialBlurWidthAdd += 0.095f * digitalRadialPower * digitalRadialBlurPower_ * demoBoost;
            modifier.radialBlurIntensityAdd += 1.10f * digitalRadialPower * digitalRadialBlurPower_ * demoBoost;
            modifier.radialSampleCountMin = (std::max)(
                modifier.radialSampleCountMin,
                digitalRadialPower > 0.0f ? 26 : 0);

            modifier.randomNoiseStrengthAdd += 0.07f * digitalNoisePower * digitalEffectPower_;
            modifier.randomNoiseScale = 1750.0f;
            modifier.randomNoiseAnimate = modifier.randomNoiseAnimate || digitalNoisePower > 0.0f;
            modifier.randomNoiseMode = 1;
        }
    }
    return modifier;
}

Vector2 GameplayEffectManager::GetViewportShakeOffset() const {
    if (!enableScreenShake_) {
        return { 0.0f, 0.0f };
    }

    const float power = GetRadialImpactPower();
    const float digitalPower = GetDigitalRadialImpactPower();
    if (power <= 0.0f && digitalPower <= 0.0f) {
        return { 0.0f, 0.0f };
    }

    const float amplitude = 8.0f * power + 6.0f * digitalPower * digitalEffectPower_;
    return {
        std::sin((harvestTimer_ + digitalTimer_) * 92.0f) * amplitude,
        std::sin((harvestTimer_ + digitalTimer_) * 137.0f) * amplitude * 0.55f,
    };
}

bool GameplayEffectManager::ConsumeHarvestParticleEmitSettings(GPUParticleEmitSettings& outSettings) {
    if (!pendingHarvestParticleEmit_) {
        return false;
    }
    pendingHarvestParticleEmit_ = false;

    outSettings = {};
    outSettings.translate = harvestPosition_;
    outSettings.radius = demoMode_ ? 1.55f : 1.15f;
    outSettings.color = { 0.78f, 1.0f, 0.28f, 1.0f };
    outSettings.scale = { 0.13f, 0.18f, 0.13f };
    outSettings.lifeTime = demoMode_ ? 1.35f : 1.05f;
    outSettings.baseVelocity = { 0.0f, 0.95f, 0.0f };
    outSettings.speed = demoMode_ ? 0.82f : 0.62f;
    const int gpuParticleCount = demoMode_ ? static_cast<int>(particleCount_ * 1.55f) : particleCount_;
    outSettings.count = static_cast<uint32_t>(std::clamp(gpuParticleCount, 1, 1024));
    outSettings.emit = 1;
    outSettings.preset = 2;
    return true;
}

bool GameplayEffectManager::ConsumeDigitalParticleEmitSettings(GPUParticleEmitSettings& outSettings) {
    if (!pendingDigitalParticleEmit_) {
        return false;
    }
    pendingDigitalParticleEmit_ = false;

    outSettings = {};
    outSettings.translate = digitalPosition_;
    outSettings.radius = demoMode_ ? 1.05f : 0.78f;
    outSettings.color = { 0.22f, 0.86f, 1.0f, 1.0f };
    outSettings.scale = demoMode_
        ? Vector3{ 0.075f, 0.075f, 0.075f }
        : Vector3{ 0.055f, 0.055f, 0.055f };
    outSettings.lifeTime = demoMode_ ? 0.82f : 0.62f;
    outSettings.baseVelocity = { 0.0f, 0.18f, 0.0f };
    outSettings.speed = demoMode_ ? 1.25f : 0.95f;

    const int requestedCount = demoMode_
        ? static_cast<int>(static_cast<float>(digitalParticleCount_) * 0.45f)
        : static_cast<int>(static_cast<float>(digitalParticleCount_) * 0.35f);
    outSettings.count = static_cast<uint32_t>(std::clamp(requestedCount, 32, 128));
    outSettings.emit = 1;
    outSettings.preset = 100;
    return true;
}

uint32_t GameplayEffectManager::GetHarvestBurstParticleCount() const {
    const int burstCount = demoMode_ ? static_cast<int>(particleCount_ * 1.55f) : particleCount_;
    return static_cast<uint32_t>(std::clamp(burstCount, 1, 1024));
}

void GameplayEffectManager::DrawGameplayEffects(
    const Vector2& viewportTopLeft,
    const Vector2& viewportSize,
    const Matrix4x4* viewProjection) const {
#ifndef USE_IMGUI
    (void)viewportTopLeft;
    (void)viewportSize;
    (void)viewProjection;
    return;
#else
    lastOverlayDrawn_ = false;
    lastViewportMin_ = viewportTopLeft;
    lastViewportSize_ = viewportSize;
    if ((!harvestActive_ && !digitalActive_) || viewportSize.x <= 1.0f || viewportSize.y <= 1.0f) {
        return;
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    if (!drawList) {
        return;
    }

    const ImVec2 clipMin{ viewportTopLeft.x, viewportTopLeft.y };
    const ImVec2 clipMax{ viewportTopLeft.x + viewportSize.x, viewportTopLeft.y + viewportSize.y };
    drawList->PushClipRect(clipMin, clipMax, true);
    const float minViewportSize = (std::min)(viewportSize.x, viewportSize.y);
    lastOverlayDrawn_ = true;

    if (digitalActive_) {
        const Vector2 digitalCenter2D = ProjectWorldToViewport(
            digitalPosition_,
            viewProjection,
            viewportTopLeft,
            viewportSize);
        lastDigitalScreenPosition_ = digitalCenter2D;
        const ImVec2 center{ digitalCenter2D.x, digitalCenter2D.y };
        const float power = GetDigitalPower();
        const float flash = std::clamp(GetDigitalFlashPower(), 0.0f, 1.0f);

        if (showDigitalForceTestOverlay_) {
            const ImVec2 viewportCenter{
                (clipMin.x + clipMax.x) * 0.5f,
                (clipMin.y + clipMax.y) * 0.5f,
            };
            drawList->AddCircle(
                viewportCenter,
                96.0f,
                IM_COL32(0, 220, 255, 255),
                64,
                10.0f);
            drawList->AddCircleFilled(
                viewportCenter,
                26.0f,
                IM_COL32(255, 255, 255, 230),
                32);
            drawList->AddLine(
                { viewportCenter.x - 150.0f, viewportCenter.y },
                { viewportCenter.x + 150.0f, viewportCenter.y },
                IM_COL32(0, 220, 255, 255),
                5.0f);
            drawList->AddLine(
                { viewportCenter.x, viewportCenter.y - 150.0f },
                { viewportCenter.x, viewportCenter.y + 150.0f },
                IM_COL32(0, 140, 255, 220),
                3.0f);
        }

        if (flash > 0.0f) {
            const float flashAlpha = flash * 205.0f;
            const float flashRadius = minViewportSize * (0.055f + 0.165f * flash);
            drawList->AddRectFilled(
                clipMin,
                clipMax,
                ColorWithAlpha(10, 70, 150, flashAlpha * 0.13f));
            drawList->AddCircleFilled(
                center,
                flashRadius,
                ColorWithAlpha(210, 248, 255, flashAlpha),
                48);
            drawList->AddCircleFilled(
                center,
                flashRadius * 1.65f,
                ColorWithAlpha(20, 160, 255, flashAlpha * 0.34f),
                64);
        }

        const float rayWindow = 0.28f;
        for (const DigitalRay& ray : digitalRays_) {
            if (digitalTimer_ < ray.delay) {
                continue;
            }
            const float rayT = Clamp01((digitalTimer_ - ray.delay) / rayWindow);
            const float rayAlpha = Squared(1.0f - rayT);
            if (rayAlpha <= 0.0f) {
                continue;
            }

            const ImVec2 direction{ std::cos(ray.angle), std::sin(ray.angle) };
            const float innerRadius = minViewportSize * 0.035f;
            const float outerRadius =
                minViewportSize *
                (0.16f + 0.52f * (1.0f - rayAlpha)) *
                ray.lengthScale *
                digitalRingScale_;
            const ImVec2 start = Add(center, Mul(direction, innerRadius));
            const ImVec2 end = Add(center, Mul(direction, outerRadius));
            drawList->AddLine(
                start,
                end,
                ColorWithAlpha(86, 226, 255, 205.0f * rayAlpha * digitalEffectPower_),
                ray.width * (0.25f + rayAlpha * 0.85f));
        }

        if (enableDigitalRing_) {
            constexpr int kSegmentCount = 96;
            for (int ringIndex = 0; ringIndex < 3; ++ringIndex) {
                const float ringDelay = static_cast<float>(ringIndex) * 0.065f;
                if (digitalTimer_ < ringDelay) {
                    continue;
                }
                const float ringT = Clamp01((digitalTimer_ - ringDelay) / digitalDuration_);
                const float ringAlpha = Squared(1.0f - ringT);
                if (ringAlpha <= 0.0f) {
                    continue;
                }

                const float radius =
                    minViewportSize *
                    digitalRingScale_ *
                    (0.045f + 0.56f * ringT + 0.032f * static_cast<float>(ringIndex));
                const float thickness =
                    (std::max)(1.5f, digitalRingThickness_ * (1.25f - 0.50f * ringT));
                for (int i = 0; i < kSegmentCount; ++i) {
                    if (((i + ringIndex) % 6) == 0 || ((i + ringIndex) % 17) == 0) {
                        continue;
                    }
                    const float angle0 = (static_cast<float>(i) / static_cast<float>(kSegmentCount)) * kPi * 2.0f;
                    const float angle1 = (static_cast<float>(i) + 0.68f) / static_cast<float>(kSegmentCount) * kPi * 2.0f;
                    const float jitter0 = ((i % 2) == 0 ? 1.025f : 0.955f);
                    const float jitter1 = ((i % 3) == 0 ? 1.055f : 0.985f);
                    const ImVec2 p0{
                        center.x + std::cos(angle0) * radius * jitter0,
                        center.y + std::sin(angle0) * radius * jitter0,
                    };
                    const ImVec2 p1{
                        center.x + std::cos(angle1) * radius * jitter1,
                        center.y + std::sin(angle1) * radius * jitter1,
                    };
                    const float alpha = 245.0f * ringAlpha * digitalEffectPower_;
                    drawList->AddLine(p0, p1, ColorWithAlpha(24, 221, 255, alpha), thickness);
                    drawList->AddLine(p0, p1, ColorWithAlpha(215, 252, 255, alpha * 0.48f), thickness * 0.42f);
                }
            }
        }

        if (enableDigitalParticles_) {
            for (const DigitalParticle& particle : digitalParticles_) {
                if (particle.lifeTime <= 0.0f || digitalTimer_ > particle.lifeTime) {
                    continue;
                }

                const float particleT = Clamp01(digitalTimer_ / particle.lifeTime);
                const float particleAlpha = Squared(1.0f - particleT);
                if (particleAlpha <= 0.0f) {
                    continue;
                }

                const ImVec2 direction{ std::cos(particle.angle), std::sin(particle.angle) };
                const ImVec2 tangent{ -direction.y, direction.x };
                const float distance =
                    minViewportSize *
                    (particle.startRadius + particle.speed * digitalTimer_ * 0.92f);
                const float jitter = std::sin(digitalTimer_ * 22.0f + particle.angle * 3.1f) * minViewportSize * 0.016f;
                const ImVec2 position = Add(
                    Add(center, Mul(direction, distance)),
                    Mul(tangent, jitter));

                const float particleAngle = particle.angle + particle.spin * digitalTimer_;
                const ImVec2 axis{ std::cos(particleAngle), std::sin(particleAngle) };
                const ImVec2 perp{ -axis.y, axis.x };
                const float halfSize =
                    particle.size *
                    (demoMode_ ? 1.22f : 1.0f) *
                    (1.0f - particleT * 0.20f);
                const ImVec2 ax = Mul(axis, halfSize);
                const ImVec2 py = Mul(perp, halfSize);
                const ImVec2 p0 = Add(Add(position, Mul(ax, -1.0f)), Mul(py, -1.0f));
                const ImVec2 p1 = Add(Add(position, ax), Mul(py, -1.0f));
                const ImVec2 p2 = Add(Add(position, ax), py);
                const ImVec2 p3 = Add(Add(position, Mul(ax, -1.0f)), py);

                ImU32 color = ColorWithAlpha(54, 210, 255, 235.0f * particleAlpha);
                if (particle.colorIndex == 1) {
                    color = ColorWithAlpha(12, 126, 255, 220.0f * particleAlpha);
                } else if (particle.colorIndex == 2) {
                    color = ColorWithAlpha(230, 252, 255, 235.0f * particleAlpha);
                }
                drawList->AddQuadFilled(p0, p1, p2, p3, color);
                drawList->AddQuad(p0, p1, p2, p3, ColorWithAlpha(225, 252, 255, 160.0f * particleAlpha), 1.0f);
            }
        }

        if (power > 0.0f) {
            const float coreRadius = minViewportSize * (0.018f + 0.026f * power);
            drawList->AddCircleFilled(center, coreRadius, ColorWithAlpha(235, 255, 255, 210.0f * power), 32);
        }
    }

    if (harvestActive_) {
        const float popupT = GetNormalizedPopupTime();
        const float alpha = Clamp01(1.0f - popupT);
        const float flash = GetFlashPower();

        if (flash > 0.0f) {
            const int flashAlpha = static_cast<int>(std::clamp(flash, 0.0f, 1.0f) * 150.0f);
            const ImVec2 center{
                viewportTopLeft.x + viewportSize.x * 0.5f,
                viewportTopLeft.y + viewportSize.y * 0.5f,
            };
            const float radius = minViewportSize * (0.20f + 0.18f * flash);
            drawList->AddRectFilled(
                clipMin,
                clipMax,
                IM_COL32(255, 246, 178, static_cast<int>(flashAlpha * 0.35f)));
            drawList->AddCircleFilled(
                center,
                radius,
                IM_COL32(255, 250, 188, flashAlpha),
                48);
        }

        if (drawHarvestPopupTextInDrawList_ && alpha > 0.0f) {
            char text[32]{};
            std::snprintf(text, sizeof(text), "+%dG", harvestPrice_);

            const float demoPopupBoost = demoMode_ ? 1.15f : 1.0f;
            const float impactScale = 1.0f + std::sin(Clamp01(harvestTimer_ / 0.20f) * kPi) * 0.45f;
            const float fontSize = ImGui::GetFontSize() * 3.5f * popupScale_ * demoPopupBoost * impactScale;
            const ImVec2 textSize = ImGui::GetFont()->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, text);
            const ImVec2 center{
                viewportTopLeft.x + viewportSize.x * 0.5f,
                viewportTopLeft.y + viewportSize.y * 0.42f - 108.0f * popupT,
            };
            const ImVec2 pos{ center.x - textSize.x * 0.5f, center.y - textSize.y * 0.5f };

            const int alphaByte = static_cast<int>(255.0f * alpha);
            const ImVec2 bgMin{ pos.x - 18.0f, pos.y - 10.0f };
            const ImVec2 bgMax{ pos.x + textSize.x + 18.0f, pos.y + textSize.y + 10.0f };
            drawList->AddRectFilled(bgMin, bgMax, IM_COL32(58, 34, 4, static_cast<int>(150.0f * alpha)), 8.0f);
            drawList->AddRect(bgMin, bgMax, IM_COL32(255, 235, 90, static_cast<int>(210.0f * alpha)), 8.0f, 0, 2.0f);

            const ImU32 outlineColor = IM_COL32(52, 31, 4, alphaByte);
            const ImU32 shadowColor = IM_COL32(40, 24, 4, alphaByte);
            const ImU32 textColor = IM_COL32(255, 248, 172, alphaByte);
            drawList->AddText(ImGui::GetFont(), fontSize, { pos.x + 4.0f, pos.y + 4.0f }, shadowColor, text);
            drawList->AddText(ImGui::GetFont(), fontSize, { pos.x - 2.0f, pos.y }, outlineColor, text);
            drawList->AddText(ImGui::GetFont(), fontSize, { pos.x + 2.0f, pos.y }, outlineColor, text);
            drawList->AddText(ImGui::GetFont(), fontSize, { pos.x, pos.y - 2.0f }, outlineColor, text);
            drawList->AddText(ImGui::GetFont(), fontSize, { pos.x, pos.y + 2.0f }, outlineColor, text);
            drawList->AddText(ImGui::GetFont(), fontSize, pos, textColor, text);
        }
    }
    drawList->PopClipRect();
#endif
}

bool GameplayEffectManager::DrawGameplayEffectImGui() {
#ifndef USE_IMGUI
    return false;
#else
    bool playRequested = false;
    ImGui::Begin("Gameplay Effect Debug");
    ImGui::Checkbox("Demo Mode", &demoMode_);
    ImGui::Checkbox("Enable Screen Shake", &enableScreenShake_);

    if (ImGui::CollapsingHeader("Harvest Impact", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::Button("Play Harvest Effect")) {
            playRequested = true;
        }
        ImGui::DragFloat3("Emit Position", &debugHarvestPosition_.x, 0.1f);
        ImGui::InputInt("Harvest Price", &debugHarvestPrice_);
        debugHarvestPrice_ = (std::max)(debugHarvestPrice_, 0);
        ImGui::Checkbox("Enable Harvest Particles", &enableParticles_);
        ImGui::SliderFloat("Harvest Effect Power", &effectPower_, 0.5f, 2.0f);
        ImGui::SliderFloat("Normal Duration", &normalDuration_, 0.4f, 0.7f);
        ImGui::SliderFloat("Demo Duration", &demoDuration_, 1.0f, 1.2f);
        ImGui::SliderFloat("Popup Duration", &popupDuration_, 0.8f, 1.2f);
        ImGui::SliderFloat("Popup Scale", &popupScale_, 0.8f, 2.0f);
        ImGui::SliderFloat("Flash Power", &flashPower_, 0.0f, 1.0f);
        ImGui::SliderInt("Particle Count", &particleCount_, 1, 1024);
    }

    if (ImGui::CollapsingHeader("Digital Impact", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::Button("Play Digital Impact Effect")) {
            PlayDigitalImpactEffect(debugDigitalPosition_);
        }
        ImGui::DragFloat3("Digital Impact Position", &debugDigitalPosition_.x, 0.1f);
        ImGui::SliderFloat("Digital Duration", &digitalNormalDuration_, 0.5f, 0.8f);
        ImGui::SliderFloat("Digital Demo Duration", &digitalDemoDuration_, 0.8f, 1.0f);
        ImGui::SliderFloat("Digital Effect Power", &digitalEffectPower_, 0.5f, 2.0f);
        ImGui::SliderFloat("Ring Scale", &digitalRingScale_, 0.5f, 2.0f);
        ImGui::SliderFloat("Ring Thickness", &digitalRingThickness_, 1.0f, 10.0f);
        ImGui::SliderInt("Digital Particle Count", &digitalParticleCount_, 0, 512);
        ImGui::SliderFloat("Digital Flash Power", &digitalFlashPower_, 0.0f, 2.0f);
        ImGui::SliderFloat("Digital RadialBlur Power", &digitalRadialBlurPower_, 0.0f, 2.0f);
        ImGui::Checkbox("Enable Digital Particles", &enableDigitalParticles_);
        ImGui::Checkbox("Enable Digital Ring", &enableDigitalRing_);
        ImGui::Checkbox("Enable Digital Screen PostEffect", &enableDigitalScreenPostEffect_);
        ImGui::Checkbox("Draw Force Test Overlay", &showDigitalForceTestOverlay_);
        ImGui::Text("Digital Active: %s", digitalActive_ ? "true" : "false");
        ImGui::Text("Digital Timer: %.3f / %.3f", digitalTimer_, digitalDuration_);
        ImGui::Text("Digital Power: %.3f", GetDigitalPower());
        ImGui::Text("Digital Generated Particles: %zu", digitalParticles_.size());
        ImGui::Text("Projected Screen Position: %.1f, %.1f",
            lastDigitalScreenPosition_.x,
            lastDigitalScreenPosition_.y);
        ImGui::Text("Viewport Min: %.1f, %.1f",
            lastViewportMin_.x,
            lastViewportMin_.y);
        ImGui::Text("Viewport Size: %.1f, %.1f",
            lastViewportSize_.x,
            lastViewportSize_.y);
        ImGui::Text("Viewport Max: %.1f, %.1f",
            lastViewportMin_.x + lastViewportSize_.x,
            lastViewportMin_.y + lastViewportSize_.y);
        ImGui::Text("Draw Overlay: %s", lastOverlayDrawn_ ? "true" : "false");
    }

    normalDuration_ = std::clamp(normalDuration_, 0.4f, 0.7f);
    demoDuration_ = std::clamp(demoDuration_, 1.0f, 1.2f);
    popupDuration_ = std::clamp(popupDuration_, 0.8f, 1.2f);
    popupScale_ = std::clamp(popupScale_, 0.8f, 2.0f);
    flashPower_ = std::clamp(flashPower_, 0.0f, 1.0f);
    effectPower_ = std::clamp(effectPower_, 0.5f, 2.0f);
    particleCount_ = std::clamp(particleCount_, 1, 1024);
    digitalNormalDuration_ = std::clamp(digitalNormalDuration_, 0.5f, 0.8f);
    digitalDemoDuration_ = std::clamp(digitalDemoDuration_, 0.8f, 1.0f);
    digitalEffectPower_ = std::clamp(digitalEffectPower_, 0.5f, 2.0f);
    digitalRingScale_ = std::clamp(digitalRingScale_, 0.5f, 2.0f);
    digitalRingThickness_ = std::clamp(digitalRingThickness_, 1.0f, 10.0f);
    digitalParticleCount_ = std::clamp(digitalParticleCount_, 0, 512);
    digitalFlashPower_ = std::clamp(digitalFlashPower_, 0.0f, 2.0f);
    digitalRadialBlurPower_ = std::clamp(digitalRadialBlurPower_, 0.0f, 2.0f);
    if (demoMode_) {
        showDigitalForceTestOverlay_ = false;
    }
    harvestDuration_ = demoMode_ ? demoDuration_ : normalDuration_;
    digitalDuration_ = demoMode_ ? digitalDemoDuration_ : digitalNormalDuration_;

    ImGui::Separator();
    ImGui::Text("Effect Timer: %.3f / %.3f", harvestTimer_, harvestDuration_);
    ImGui::Text("Harvest Power: %.3f", GetHarvestPower());
    ImGui::Text("Effective Particle Count: %u", GetHarvestBurstParticleCount());

    const ScreenPostEffectModifier modifier = GetScreenPostEffectModifier();
    ImGui::Separator();
    ImGui::Text("HSV +Sat/+Val: %.3f / %.3f", modifier.hsvSaturationAdd, modifier.hsvValueAdd);
    ImGui::Text("Vignette +Scale/+Intensity: %.3f / %.3f",
        modifier.vignetteScaleAdd,
        modifier.vignetteIntensityAdd);
    ImGui::Text("Radial +Width/+Intensity: %.3f / %.3f",
        modifier.radialBlurWidthAdd,
        modifier.radialBlurIntensityAdd);
    ImGui::Text("RandomNoise +Strength: %.3f", modifier.randomNoiseStrengthAdd);
    ImGui::End();
    return playRequested;
#endif
}
