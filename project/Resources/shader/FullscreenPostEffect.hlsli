cbuffer FullscreenPostEffectParameter : register(b0)
{
    float32_t grayscaleIntensity;
    float32_t sepiaIntensity;
    float32_t blurStrength;
    float32_t padding;
    float32_t bloomThreshold;
    float32_t bloomIntensity;
    float32_t bloomRadius;
    float32_t bloomSoftKnee;
};
