#include "CopyImage.hlsli"
#include "FullscreenPostEffect.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

float GaussianWeight(float distanceSquared, float sigma)
{
    float sigmaSquared = sigma * sigma;
    return exp(-distanceSquared / (2.0f * sigmaSquared));
}

float4 main(VertexShaderOutput input) : SV_TARGET0
{
    uint32_t width;
    uint32_t height;
    gTexture.GetDimensions(width, height);

    float2 texelSize = 1.0f / float2(max(width, 1u), max(height, 1u));
    float sigma = max(gaussianSigma, 0.1f);
    float4 accumulatedColor = 0.0f;
    float accumulatedWeight = 0.0f;

    [unroll]
    for (int32_t y = -2; y <= 2; ++y)
    {
        [unroll]
        for (int32_t x = -2; x <= 2; ++x)
        {
            float2 sampleOffset = float2(x, y);
            float weight = GaussianWeight(dot(sampleOffset, sampleOffset), sigma);
            accumulatedColor +=
                gTexture.Sample(gSampler, input.texcoord + sampleOffset * texelSize) * weight;
            accumulatedWeight += weight;
        }
    }

    return accumulatedColor / max(accumulatedWeight, 0.0001f);
}
