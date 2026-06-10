#include "CopyImage.hlsli"
#include "FullscreenPostEffect.hlsli"

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

float32_t4 main(VertexShaderOutput input) : SV_TARGET0
{
    float32_t4 color = gTexture.Sample(gSampler, input.texcoord);
    float32_t value = dot(color.rgb, float32_t3(0.2125f, 0.7154f, 0.0721f));
    float32_t intensity = saturate(grayscaleIntensity);
    return float32_t4(lerp(color.rgb, float32_t3(value, value, value), intensity), color.a);
}
