#include "CopyImage.hlsli"
#include "FullscreenPostEffect.hlsli"

Texture2D<float32_t4> gTexture : register(t0);
Texture2D<float32_t> gDepthTexture : register(t1);
Texture2D<float32_t4> gNormalTexture : register(t2);
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

float32_t3 DecodeNormal(float32_t2 texcoord)
{
    float32_t3 normal = gNormalTexture.Sample(gSampler, texcoord).rgb * 2.0f - 1.0f;
    float32_t lengthSq = dot(normal, normal);
    return lengthSq > 0.00001f ? normal * rsqrt(lengthSq) : float32_t3(0.0f, 0.0f, 1.0f);
}

float32_t CalculateDepthOutline(VertexShaderOutput input)
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
    return smoothstep(threshold, threshold + edgeWidth, edge) * max(depthOutlineIntensity, 0.0f);
}

float32_t CalculateNormalOutline(VertexShaderOutput input)
{
    uint32_t width;
    uint32_t height;
    gNormalTexture.GetDimensions(width, height);

    float32_t thickness = max(normalOutlineThickness, 1.0f);
    float32_t2 texelSize = thickness / float32_t2(max(width, 1u), max(height, 1u));

    float32_t3 n00 = DecodeNormal(input.texcoord + texelSize * float32_t2(-1.0f, -1.0f));
    float32_t3 n10 = DecodeNormal(input.texcoord + texelSize * float32_t2( 0.0f, -1.0f));
    float32_t3 n20 = DecodeNormal(input.texcoord + texelSize * float32_t2( 1.0f, -1.0f));
    float32_t3 n01 = DecodeNormal(input.texcoord + texelSize * float32_t2(-1.0f,  0.0f));
    float32_t3 n21 = DecodeNormal(input.texcoord + texelSize * float32_t2( 1.0f,  0.0f));
    float32_t3 n02 = DecodeNormal(input.texcoord + texelSize * float32_t2(-1.0f,  1.0f));
    float32_t3 n12 = DecodeNormal(input.texcoord + texelSize * float32_t2( 0.0f,  1.0f));
    float32_t3 n22 = DecodeNormal(input.texcoord + texelSize * float32_t2( 1.0f,  1.0f));

    float32_t3 sobelX = -n00 - 2.0f * n01 - n02 + n20 + 2.0f * n21 + n22;
    float32_t3 sobelY = -n00 - 2.0f * n10 - n20 + n02 + 2.0f * n12 + n22;
    float32_t edge = sqrt(dot(sobelX, sobelX) + dot(sobelY, sobelY));

    float32_t threshold = max(normalOutlineThreshold, 0.0f);
    float32_t edgeWidth = max(threshold * 0.5f, 0.001f);
    return smoothstep(threshold, threshold + edgeWidth, edge) * max(normalOutlineIntensity, 0.0f);
}

float32_t4 main(VertexShaderOutput input) : SV_TARGET0
{
    float32_t depthOutline = CalculateDepthOutline(input);
    float32_t normalOutline = CalculateNormalOutline(input);
    float32_t outline = saturate(max(depthOutline, normalOutline));

    float32_t4 color = gTexture.Sample(gSampler, input.texcoord);
    float32_t3 outlinedColor = lerp(color.rgb, float32_t3(0.0f, 0.0f, 0.0f), outline);
    return float32_t4(outlinedColor, color.a);
}
