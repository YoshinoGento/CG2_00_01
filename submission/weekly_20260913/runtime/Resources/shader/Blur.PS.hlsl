#include "CopyImage.hlsli"
#include "FullscreenPostEffect.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

float4 main(VertexShaderOutput input) : SV_TARGET0
{
    uint32_t width;
    uint32_t height;
    gTexture.GetDimensions(width, height);

    float strength = max(blurStrength, 0.0f);
    float4 center = gTexture.Sample(gSampler, input.texcoord);

    if (strength <= 0.001f) {
        return center;
    }

    float2 texelSize = strength / float2(max(width, 1u), max(height, 1u));
    float3 color = float3(0.0f, 0.0f, 0.0f);
    float weightSum = 0.0f;

    [unroll]
    for (int32_t y = -4; y <= 4; ++y) {
        [unroll]
        for (int32_t x = -4; x <= 4; ++x) {
            float2 offset = float2((float)x, (float)y);
            float weight = 1.0f;
            color += gTexture.Sample(gSampler, input.texcoord + texelSize * offset).rgb * weight;
            weightSum += weight;
        }
    }

    float3 blurred = color / max(weightSum, 1.0f);
    float intensity = saturate(strength / 2.0f);
    return float4(lerp(center.rgb, blurred, intensity), center.a);
}
