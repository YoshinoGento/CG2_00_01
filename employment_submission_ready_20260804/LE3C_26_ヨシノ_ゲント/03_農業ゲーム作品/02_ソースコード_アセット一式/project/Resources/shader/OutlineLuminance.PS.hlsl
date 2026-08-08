#include "CopyImage.hlsli"
#include "FullscreenPostEffect.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

static const float3 kLuminance = float3(0.2125f, 0.7154f, 0.0721f);

float SampleLuminance(float2 texcoord)
{
    return dot(gTexture.Sample(gSampler, texcoord).rgb, kLuminance);
}

float4 main(VertexShaderOutput input) : SV_TARGET0
{
    uint32_t width;
    uint32_t height;
    gTexture.GetDimensions(width, height);

    float thickness = max(outlineThickness, 0.0f);
    float2 texelSize = thickness / float2(max(width, 1u), max(height, 1u));

    float l00 = SampleLuminance(input.texcoord + texelSize * float2(-1.0f, -1.0f));
    float l10 = SampleLuminance(input.texcoord + texelSize * float2( 0.0f, -1.0f));
    float l20 = SampleLuminance(input.texcoord + texelSize * float2( 1.0f, -1.0f));
    float l01 = SampleLuminance(input.texcoord + texelSize * float2(-1.0f,  0.0f));
    float l21 = SampleLuminance(input.texcoord + texelSize * float2( 1.0f,  0.0f));
    float l02 = SampleLuminance(input.texcoord + texelSize * float2(-1.0f,  1.0f));
    float l12 = SampleLuminance(input.texcoord + texelSize * float2( 0.0f,  1.0f));
    float l22 = SampleLuminance(input.texcoord + texelSize * float2( 1.0f,  1.0f));

    float sobelX = -l00 - 2.0f * l01 - l02 + l20 + 2.0f * l21 + l22;
    float sobelY = -l00 - 2.0f * l10 - l20 + l02 + 2.0f * l12 + l22;
    float edge = length(float2(sobelX, sobelY));

    float threshold = max(outlineThreshold, 0.0f);
    float edgeWidth = max(threshold * 0.5f, 0.001f);
    float outline = smoothstep(threshold, threshold + edgeWidth, edge);
    outline *= max(outlineIntensity, 0.0f);

    float4 color = gTexture.Sample(gSampler, input.texcoord);
    float3 outlinedColor = lerp(color.rgb, float3(0.0f, 0.0f, 0.0f), saturate(outline));
    return float4(outlinedColor, color.a);
}
