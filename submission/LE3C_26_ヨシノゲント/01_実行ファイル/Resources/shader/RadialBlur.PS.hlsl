#include "CopyImage.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct RadialBlurParam
{
    float2 center;
    float blurWidth;
    float intensity;

    int32_t sampleCount;
    float3 padding;
};

ConstantBuffer<RadialBlurParam> gRadialBlurParam : register(b0);

float4 main(VertexShaderOutput input) : SV_TARGET0
{
    float4 original = gTexture.Sample(gSampler, input.texcoord);
    int32_t sampleCount = clamp(gRadialBlurParam.sampleCount, 1, 32);
    float blurWidth = max(gRadialBlurParam.blurWidth, 0.0f);
    float intensity = saturate(gRadialBlurParam.intensity);

    if (sampleCount <= 1 || blurWidth <= 0.000001f || intensity <= 0.000001f) {
        return original;
    }

    float2 direction = input.texcoord - gRadialBlurParam.center;
    float4 blurred = float4(0.0f, 0.0f, 0.0f, 0.0f);
    float denominator = max(float(sampleCount - 1), 1.0f);

    [loop]
    for (int32_t i = 0; i < sampleCount; ++i) {
        float t = float(i) / denominator;
        float2 sampleUv = input.texcoord - direction * blurWidth * t;
        blurred += gTexture.Sample(gSampler, sampleUv);
    }

    blurred /= float(sampleCount);
    return float4(lerp(original.rgb, blurred.rgb, intensity), original.a);
}
