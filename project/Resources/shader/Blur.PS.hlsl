#include "CopyImage.hlsli"
#include "FullscreenPostEffect.hlsli"

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

float32_t4 main(VertexShaderOutput input) : SV_TARGET0
{
    uint32_t width;
    uint32_t height;
    gTexture.GetDimensions(width, height);

    float32_t strength = max(blurStrength, 0.0f);
    float32_t4 center = gTexture.Sample(gSampler, input.texcoord);

    if (strength <= 0.001f) {
        return center;
    }

    float32_t2 texelSize = strength / float32_t2(max(width, 1u), max(height, 1u));
    float32_t3 color = float32_t3(0.0f, 0.0f, 0.0f);
    float32_t weightSum = 0.0f;

    [unroll]
    for (int32_t y = -4; y <= 4; ++y) {
        [unroll]
        for (int32_t x = -4; x <= 4; ++x) {
            float32_t2 offset = float32_t2((float32_t)x, (float32_t)y);
            float32_t weight = 1.0f;
            color += gTexture.Sample(gSampler, input.texcoord + texelSize * offset).rgb * weight;
            weightSum += weight;
        }
    }

    float32_t3 blurred = color / max(weightSum, 1.0f);
    float32_t intensity = saturate(strength / 2.0f);
    return float32_t4(lerp(center.rgb, blurred, intensity), center.a);
}
