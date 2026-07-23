#include "CopyImage.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

static const float kKernel3x3[3][3] = {
    { 1.0f / 9.0f, 1.0f / 9.0f, 1.0f / 9.0f },
    { 1.0f / 9.0f, 1.0f / 9.0f, 1.0f / 9.0f },
    { 1.0f / 9.0f, 1.0f / 9.0f, 1.0f / 9.0f },
};

static const float2 kIndex3x3[3][3] = {
    { { -1.0f, -1.0f }, { 0.0f, -1.0f }, { 1.0f, -1.0f } },
    { { -1.0f,  0.0f }, { 0.0f,  0.0f }, { 1.0f,  0.0f } },
    { { -1.0f,  1.0f }, { 0.0f,  1.0f }, { 1.0f,  1.0f } },
};

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    uint32_t width;
    uint32_t height;
    gTexture.GetDimensions(width, height);

    float2 uvStepSize = float2(rcp(float(width)), rcp(float(height)));
    float3 result = float3(0.0f, 0.0f, 0.0f);
    float alpha = 0.0f;

    [unroll]
    for (int32_t x = 0; x < 3; ++x) {
        [unroll]
        for (int32_t y = 0; y < 3; ++y) {
            float2 texcoord = input.texcoord + kIndex3x3[x][y] * uvStepSize;
            float4 sampleColor = gTexture.Sample(gSampler, texcoord);
            result += sampleColor.rgb * kKernel3x3[x][y];
            alpha += sampleColor.a * kKernel3x3[x][y];
        }
    }

    output.color = float4(result, alpha);
    return output;
}
