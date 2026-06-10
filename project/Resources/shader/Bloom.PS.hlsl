#include "CopyImage.hlsli"
#include "FullscreenPostEffect.hlsli"

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

float32_t4 main(VertexShaderOutput input) : SV_TARGET0
{
    uint32_t width;
    uint32_t height;
    gTexture.GetDimensions(width, height);

    float32_t4 center = gTexture.Sample(gSampler, input.texcoord);
    float32_t intensity = max(bloomIntensity, 0.0f);
    float32_t radius = max(bloomRadius, 0.0f);
    if (intensity <= 0.001f || radius <= 0.001f) {
        return center;
    }

    float32_t threshold = saturate(bloomThreshold);
    float32_t softKnee = max(bloomSoftKnee, 0.001f);
    float32_t2 texelSize = radius / float32_t2(max(width, 1u), max(height, 1u));
    float32_t3 bloom = float32_t3(0.0f, 0.0f, 0.0f);
    float32_t weightSum = 0.0f;

    [unroll]
    for (int32_t y = -4; y <= 4; ++y) {
        [unroll]
        for (int32_t x = -4; x <= 4; ++x) {
            float32_t2 offset = float32_t2((float32_t)x, (float32_t)y);
            float32_t normalizedDistance = length(offset) / 5.657f;
            float32_t weight = saturate(1.0f - normalizedDistance);
            float32_t3 sampleColor = gTexture.Sample(gSampler, input.texcoord + texelSize * offset).rgb;
            float32_t luminance = dot(sampleColor, float32_t3(0.2125f, 0.7154f, 0.0721f));
            float32_t bright = smoothstep(threshold, threshold + softKnee, luminance);
            bloom += sampleColor * bright * weight;
            weightSum += weight;
        }
    }

    bloom /= max(weightSum, 1.0f);
    float32_t3 finalColor = center.rgb + bloom * intensity;
    return float32_t4(saturate(finalColor), center.a);
}
