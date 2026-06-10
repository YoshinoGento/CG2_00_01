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
    float32_t outlineThreshold;
    float32_t outlineIntensity;
    float32_t outlineThickness;
    float32_t outlinePadding;
    float32_t depthOutlineThreshold;
    float32_t depthOutlineIntensity;
    float32_t depthOutlineThickness;
    float32_t depthOutlinePadding;
    float32_t depthOutlineNearClip;
    float32_t depthOutlineFarClip;
    float32_t depthOutlineLinearize;
    float32_t depthOutlineLinearPadding;
    float32_t normalOutlineThreshold;
    float32_t normalOutlineIntensity;
    float32_t normalOutlineThickness;
    float32_t normalOutlinePadding;
};
