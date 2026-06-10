#include "CopyImage.hlsli"
#include "FullscreenPostEffect.hlsli"

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

static const float32_t3 kLuminance = float32_t3(0.2125f, 0.7154f, 0.0721f);

float32_t SampleLuminance(float32_t2 texcoord)
{
    return dot(gTexture.Sample(gSampler, texcoord).rgb, kLuminance);
}

float32_t4 main(VertexShaderOutput input) : SV_TARGET0
{
    uint32_t width;
    uint32_t height;
    gTexture.GetDimensions(width, height);

    float32_t thickness = max(outlineThickness, 0.0f);
    float32_t2 texelSize = thickness / float32_t2(max(width, 1u), max(height, 1u));

    float32_t l00 = SampleLuminance(input.texcoord + texelSize * float32_t2(-1.0f, -1.0f));
    float32_t l10 = SampleLuminance(input.texcoord + texelSize * float32_t2( 0.0f, -1.0f));
    float32_t l20 = SampleLuminance(input.texcoord + texelSize * float32_t2( 1.0f, -1.0f));
    float32_t l01 = SampleLuminance(input.texcoord + texelSize * float32_t2(-1.0f,  0.0f));
    float32_t l21 = SampleLuminance(input.texcoord + texelSize * float32_t2( 1.0f,  0.0f));
    float32_t l02 = SampleLuminance(input.texcoord + texelSize * float32_t2(-1.0f,  1.0f));
    float32_t l12 = SampleLuminance(input.texcoord + texelSize * float32_t2( 0.0f,  1.0f));
    float32_t l22 = SampleLuminance(input.texcoord + texelSize * float32_t2( 1.0f,  1.0f));

    float32_t sobelX = -l00 - 2.0f * l01 - l02 + l20 + 2.0f * l21 + l22;
    float32_t sobelY = -l00 - 2.0f * l10 - l20 + l02 + 2.0f * l12 + l22;
    float32_t edge = length(float32_t2(sobelX, sobelY));

    float32_t threshold = max(outlineThreshold, 0.0f);
    float32_t edgeWidth = max(threshold * 0.5f, 0.001f);
    float32_t outline = smoothstep(threshold, threshold + edgeWidth, edge);
    outline *= max(outlineIntensity, 0.0f);

    float32_t4 color = gTexture.Sample(gSampler, input.texcoord);
    float32_t3 outlinedColor = lerp(color.rgb, float32_t3(0.0f, 0.0f, 0.0f), saturate(outline));
    return float32_t4(outlinedColor, color.a);
}
