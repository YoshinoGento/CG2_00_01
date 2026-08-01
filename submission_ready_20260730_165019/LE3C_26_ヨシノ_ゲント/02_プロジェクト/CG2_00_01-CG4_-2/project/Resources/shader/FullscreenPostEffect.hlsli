cbuffer FullscreenPostEffectParameter : register(b0)
{
    float grayscaleIntensity;
    float sepiaIntensity;
    float blurStrength;
    float padding;
    float bloomThreshold;
    float bloomIntensity;
    float bloomRadius;
    float bloomSoftKnee;
    float outlineThreshold;
    float outlineIntensity;
    float outlineThickness;
    float outlinePadding;
    float depthOutlineThreshold;
    float depthOutlineIntensity;
    float depthOutlineThickness;
    float depthOutlinePadding;
    float depthOutlineNearClip;
    float depthOutlineFarClip;
    float depthOutlineLinearize;
    float depthOutlineLinearPadding;
    float normalOutlineThreshold;
    float normalOutlineIntensity;
    float normalOutlineThickness;
    float normalOutlinePadding;
};
