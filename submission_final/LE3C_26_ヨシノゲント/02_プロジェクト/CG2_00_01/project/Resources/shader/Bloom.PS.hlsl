#include "CopyImage.hlsli"
#include "FullscreenPostEffect.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

float4 main(VertexShaderOutput input) : SV_TARGET0
{
    uint32_t width;
    uint32_t height;
    gTexture.GetDimensions(width, height);

    float4 center = gTexture.Sample(gSampler, input.texcoord);
    float intensity = max(bloomIntensity, 0.0f);
    float radius = max(bloomRadius, 0.0f);
    if (intensity <= 0.001f || radius <= 0.001f) {
        return center;
    }

    float threshold = saturate(bloomThreshold);
    float softKnee = max(bloomSoftKnee, 0.001f);
    float2 texelSize = radius / float2(max(width, 1u), max(height, 1u));
    float3 bloom = float3(0.0f, 0.0f, 0.0f);
    float weightSum = 0.0f;

    [unroll]
    for (int32_t y = -4; y <= 4; ++y) {
        [unroll]
        for (int32_t x = -4; x <= 4; ++x) {
            float2 offset = float2((float)x, (float)y);
            float normalizedDistance = length(offset) / 5.657f;
            float weight = saturate(1.0f - normalizedDistance);
            float3 sampleColor = gTexture.Sample(gSampler, input.texcoord + texelSize * offset).rgb;
            float luminance = dot(sampleColor, float3(0.2125f, 0.7154f, 0.0721f));
            float bright = smoothstep(threshold, threshold + softKnee, luminance);
            bloom += sampleColor * bright * weight;
            weightSum += weight;
        }
    }

    bloom /= max(weightSum, 1.0f);
    float3 finalColor = center.rgb + bloom * intensity;
    return float4(saturate(finalColor), center.a);
}
