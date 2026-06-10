#include "CopyImage.hlsli"
#include "FullscreenPostEffect.hlsli"

Texture2D<float32_t4> gTexture : register(t0);
Texture2D<float32_t> gDepthTexture : register(t1);
SamplerState gSampler : register(s0);

float32_t LinearizeDepth(float32_t depth)
{
    float32_t nearClip = max(depthOutlineNearClip, 0.0001f);
    float32_t farClip = max(depthOutlineFarClip, nearClip + 0.0001f);
    float32_t linearViewZ = (nearClip * farClip) / max(farClip - depth * (farClip - nearClip), 0.0001f);
    return linearViewZ / farClip;
}

float32_t SampleDepth(int32_t2 pixel, int32_t2 offset, int32_t2 textureSize)
{
    int32_t2 samplePixel = clamp(pixel + offset, int32_t2(0, 0), textureSize - int32_t2(1, 1));
    float32_t depth = gDepthTexture.Load(int32_t3(samplePixel, 0));
    return depthOutlineLinearize != 0.0f ? LinearizeDepth(depth) : depth;
}

float32_t4 main(VertexShaderOutput input) : SV_TARGET0
{
    uint32_t width;
    uint32_t height;
    gDepthTexture.GetDimensions(width, height);

    int32_t2 textureSize = int32_t2(max(width, 1u), max(height, 1u));
    int32_t2 pixel = int32_t2(input.texcoord * float32_t2(textureSize));
    pixel = clamp(pixel, int32_t2(0, 0), textureSize - int32_t2(1, 1));

    int32_t thickness = max(1, int32_t(round(max(depthOutlineThickness, 1.0f))));

    float32_t d00 = SampleDepth(pixel, int32_t2(-thickness, -thickness), textureSize);
    float32_t d10 = SampleDepth(pixel, int32_t2(0, -thickness), textureSize);
    float32_t d20 = SampleDepth(pixel, int32_t2(thickness, -thickness), textureSize);
    float32_t d01 = SampleDepth(pixel, int32_t2(-thickness, 0), textureSize);
    float32_t d21 = SampleDepth(pixel, int32_t2(thickness, 0), textureSize);
    float32_t d02 = SampleDepth(pixel, int32_t2(-thickness, thickness), textureSize);
    float32_t d12 = SampleDepth(pixel, int32_t2(0, thickness), textureSize);
    float32_t d22 = SampleDepth(pixel, int32_t2(thickness, thickness), textureSize);

    float32_t sobelX = -d00 - 2.0f * d01 - d02 + d20 + 2.0f * d21 + d22;
    float32_t sobelY = -d00 - 2.0f * d10 - d20 + d02 + 2.0f * d12 + d22;
    float32_t edge = length(float32_t2(sobelX, sobelY));

    float32_t threshold = max(depthOutlineThreshold, 0.0f);
    float32_t edgeWidth = max(threshold * 2.0f, 0.00001f);
    float32_t outline = smoothstep(threshold, threshold + edgeWidth, edge);
    outline *= max(depthOutlineIntensity, 0.0f);

    float32_t4 color = gTexture.Sample(gSampler, input.texcoord);
    float32_t3 outlinedColor = lerp(color.rgb, float32_t3(0.0f, 0.0f, 0.0f), saturate(outline));
    return float32_t4(outlinedColor, color.a);
}
