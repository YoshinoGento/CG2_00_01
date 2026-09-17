#include "CopyImage.hlsli"
#include "FullscreenPostEffect.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

float4 main(VertexShaderOutput input) : SV_TARGET0
{
    static const float3 kLuminance = float3(0.2125f, 0.7154f, 0.0721f);
    static const float3 kSepiaTint = float3(1.0f, 74.0f / 107.0f, 43.0f / 107.0f);

    float4 color = gTexture.Sample(gSampler, input.texcoord);
    float value = dot(color.rgb, kLuminance);
    float3 sepia = saturate(value * kSepiaTint);
    float intensity = saturate(sepiaIntensity);
    return float4(lerp(color.rgb, sepia, intensity), color.a);
}
