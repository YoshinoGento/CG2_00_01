#include "CopyImage.hlsli"
#include "FullscreenPostEffect.hlsli"

Texture2D<float32_t4> gTexture : register(t0);
Texture2D<float32_t4> gNormalTexture : register(t2);
SamplerState gSampler : register(s0);

float32_t3 DecodeNormal(float32_t2 texcoord)
{
    float32_t3 normal = gNormalTexture.Sample(gSampler, texcoord).rgb * 2.0f - 1.0f;
    float32_t lengthSq = dot(normal, normal);
    return lengthSq > 0.00001f ? normal * rsqrt(lengthSq) : float32_t3(0.0f, 0.0f, 1.0f);
}

float32_t4 main(VertexShaderOutput input) : SV_TARGET0
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
    float32_t outline = smoothstep(threshold, threshold + edgeWidth, edge);
    outline *= max(normalOutlineIntensity, 0.0f);

    float32_t4 color = gTexture.Sample(gSampler, input.texcoord);
    float32_t3 outlinedColor = lerp(color.rgb, float32_t3(0.0f, 0.0f, 0.0f), saturate(outline));
    return float32_t4(outlinedColor, color.a);
}
