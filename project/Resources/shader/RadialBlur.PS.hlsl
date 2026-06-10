#include "CopyImage.hlsli"

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct RadialBlurParam
{
    float32_t2 center;
    float32_t blurWidth;
    float32_t intensity;

    int32_t sampleCount;
    float32_t3 padding;
};

ConstantBuffer<RadialBlurParam> gRadialBlurParam : register(b0);

float32_t4 main(VertexShaderOutput input) : SV_TARGET0
{
    float32_t4 original = gTexture.Sample(gSampler, input.texcoord);
    int32_t sampleCount = clamp(gRadialBlurParam.sampleCount, 1, 32);
    float32_t blurWidth = max(gRadialBlurParam.blurWidth, 0.0f);
    float32_t intensity = saturate(gRadialBlurParam.intensity);

    if (sampleCount <= 1 || blurWidth <= 0.000001f || intensity <= 0.000001f) {
        return original;
    }

    float32_t2 direction = input.texcoord - gRadialBlurParam.center;
    float32_t4 blurred = float32_t4(0.0f, 0.0f, 0.0f, 0.0f);
    float32_t denominator = max(float32_t(sampleCount - 1), 1.0f);

    [loop]
    for (int32_t i = 0; i < sampleCount; ++i) {
        float32_t t = float32_t(i) / denominator;
        float32_t2 sampleUv = input.texcoord - direction * blurWidth * t;
        blurred += gTexture.Sample(gSampler, sampleUv);
    }

    blurred /= float32_t(sampleCount);
    return float32_t4(lerp(original.rgb, blurred.rgb, intensity), original.a);
}
