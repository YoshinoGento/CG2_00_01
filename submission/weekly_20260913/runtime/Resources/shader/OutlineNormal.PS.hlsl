#include "CopyImage.hlsli"
#include "FullscreenPostEffect.hlsli"

Texture2D<float4> gTexture : register(t0);
Texture2D<float4> gNormalTexture : register(t2);
SamplerState gSampler : register(s0);

float3 DecodeNormal(float2 texcoord)
{
    float3 normal = gNormalTexture.Sample(gSampler, texcoord).rgb * 2.0f - 1.0f;
    float lengthSq = dot(normal, normal);
    return lengthSq > 0.00001f ? normal * rsqrt(lengthSq) : float3(0.0f, 0.0f, 1.0f);
}

float4 main(VertexShaderOutput input) : SV_TARGET0
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
    float outline = smoothstep(threshold, threshold + edgeWidth, edge);
    outline *= max(normalOutlineIntensity, 0.0f);

    float4 color = gTexture.Sample(gSampler, input.texcoord);
    float3 outlinedColor = lerp(color.rgb, float3(0.0f, 0.0f, 0.0f), saturate(outline));
    return float4(outlinedColor, color.a);
}
