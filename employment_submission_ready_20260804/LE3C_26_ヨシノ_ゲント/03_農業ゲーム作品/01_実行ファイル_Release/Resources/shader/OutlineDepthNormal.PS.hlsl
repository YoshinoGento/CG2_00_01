#include "CopyImage.hlsli"
#include "FullscreenPostEffect.hlsli"

Texture2D<float4> gTexture : register(t0);
Texture2D<float> gDepthTexture : register(t1);
Texture2D<float4> gNormalTexture : register(t2);
SamplerState gSampler : register(s0);

float LinearizeDepth(float depth)
{
    float nearClip = max(depthOutlineNearClip, 0.0001f);
    float farClip = max(depthOutlineFarClip, nearClip + 0.0001f);
    float linearViewZ = (nearClip * farClip) / max(farClip - depth * (farClip - nearClip), 0.0001f);
    return linearViewZ / farClip;
}

float SampleDepth(int32_t2 pixel, int32_t2 offset, int32_t2 textureSize)
{
    int32_t2 samplePixel = clamp(pixel + offset, int32_t2(0, 0), textureSize - int32_t2(1, 1));
    float depth = gDepthTexture.Load(int32_t3(samplePixel, 0));
    return depthOutlineLinearize != 0.0f ? LinearizeDepth(depth) : depth;
}

float3 DecodeNormal(float2 texcoord)
{
    float3 normal = gNormalTexture.Sample(gSampler, texcoord).rgb * 2.0f - 1.0f;
    float lengthSq = dot(normal, normal);
    return lengthSq > 0.00001f ? normal * rsqrt(lengthSq) : float3(0.0f, 0.0f, 1.0f);
}

float CalculateDepthOutline(VertexShaderOutput input)
{
    uint32_t width;
    uint32_t height;
    gDepthTexture.GetDimensions(width, height);

    int32_t2 textureSize = int32_t2(max(width, 1u), max(height, 1u));
    int32_t2 pixel = int32_t2(input.texcoord * float2(textureSize));
    pixel = clamp(pixel, int32_t2(0, 0), textureSize - int32_t2(1, 1));

    int32_t thickness = max(1, int32_t(round(max(depthOutlineThickness, 1.0f))));

    float d00 = SampleDepth(pixel, int32_t2(-thickness, -thickness), textureSize);
    float d10 = SampleDepth(pixel, int32_t2(0, -thickness), textureSize);
    float d20 = SampleDepth(pixel, int32_t2(thickness, -thickness), textureSize);
    float d01 = SampleDepth(pixel, int32_t2(-thickness, 0), textureSize);
    float d21 = SampleDepth(pixel, int32_t2(thickness, 0), textureSize);
    float d02 = SampleDepth(pixel, int32_t2(-thickness, thickness), textureSize);
    float d12 = SampleDepth(pixel, int32_t2(0, thickness), textureSize);
    float d22 = SampleDepth(pixel, int32_t2(thickness, thickness), textureSize);

    float sobelX = -d00 - 2.0f * d01 - d02 + d20 + 2.0f * d21 + d22;
    float sobelY = -d00 - 2.0f * d10 - d20 + d02 + 2.0f * d12 + d22;
    float edge = length(float2(sobelX, sobelY));

    float threshold = max(depthOutlineThreshold, 0.0f);
    float edgeWidth = max(threshold * 2.0f, 0.00001f);
    return smoothstep(threshold, threshold + edgeWidth, edge) * max(depthOutlineIntensity, 0.0f);
}

float CalculateNormalOutline(VertexShaderOutput input)
{
    uint32_t width;
    uint32_t height;
    gNormalTexture.GetDimensions(width, height);

    float thickness = max(normalOutlineThickness, 1.0f);
    float2 texelSize = thickness / float2(max(width, 1u), max(height, 1u));

    float3 n00 = DecodeNormal(input.texcoord + texelSize * float2(-1.0f, -1.0f));
    float3 n10 = DecodeNormal(input.texcoord + texelSize * float2( 0.0f, -1.0f));
    float3 n20 = DecodeNormal(input.texcoord + texelSize * float2( 1.0f, -1.0f));
    float3 n01 = DecodeNormal(input.texcoord + texelSize * float2(-1.0f,  0.0f));
    float3 n21 = DecodeNormal(input.texcoord + texelSize * float2( 1.0f,  0.0f));
    float3 n02 = DecodeNormal(input.texcoord + texelSize * float2(-1.0f,  1.0f));
    float3 n12 = DecodeNormal(input.texcoord + texelSize * float2( 0.0f,  1.0f));
    float3 n22 = DecodeNormal(input.texcoord + texelSize * float2( 1.0f,  1.0f));

    float3 sobelX = -n00 - 2.0f * n01 - n02 + n20 + 2.0f * n21 + n22;
    float3 sobelY = -n00 - 2.0f * n10 - n20 + n02 + 2.0f * n12 + n22;
    float edge = sqrt(dot(sobelX, sobelX) + dot(sobelY, sobelY));

    float threshold = max(normalOutlineThreshold, 0.0f);
    float edgeWidth = max(threshold * 0.5f, 0.001f);
    return smoothstep(threshold, threshold + edgeWidth, edge) * max(normalOutlineIntensity, 0.0f);
}

float4 main(VertexShaderOutput input) : SV_TARGET0
{
    float depthOutline = CalculateDepthOutline(input);
    float normalOutline = CalculateNormalOutline(input);
    float outline = saturate(max(depthOutline, normalOutline));

    float4 color = gTexture.Sample(gSampler, input.texcoord);
    float3 outlinedColor = lerp(color.rgb, float3(0.0f, 0.0f, 0.0f), outline);
    return float4(outlinedColor, color.a);
}
