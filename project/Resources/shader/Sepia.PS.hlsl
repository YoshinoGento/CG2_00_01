#include "CopyImage.hlsli"
#include "FullscreenPostEffect.hlsli"

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

float32_t4 main(VertexShaderOutput input) : SV_TARGET0
{
    static const float32_t3 kLuminance = float32_t3(0.2125f, 0.7154f, 0.0721f);
    static const float32_t3 kSepiaTint = float32_t3(1.0f, 74.0f / 107.0f, 43.0f / 107.0f);

    float32_t4 color = gTexture.Sample(gSampler, input.texcoord);
    float32_t value = dot(color.rgb, kLuminance);
    float32_t3 sepia = saturate(value * kSepiaTint);
    float32_t intensity = saturate(sepiaIntensity);
    return float32_t4(lerp(color.rgb, sepia, intensity), color.a);
}
