#include "CopyImage.hlsli"
#include "FullscreenPostEffect.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

float4 main(VertexShaderOutput input) : SV_TARGET0
{
    float4 color = gTexture.Sample(gSampler, input.texcoord);
    float value = dot(color.rgb, float3(0.2125f, 0.7154f, 0.0721f));
    float intensity = saturate(grayscaleIntensity);
    return float4(lerp(color.rgb, float3(value, value, value), intensity), color.a);
}
