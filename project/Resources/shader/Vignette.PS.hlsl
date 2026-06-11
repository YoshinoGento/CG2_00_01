#include "CopyImage.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct VignetteParam
{
    float scale;
    float power;
    float intensity;
    float padding;
};

ConstantBuffer<VignetteParam> gVignetteParam : register(b0);

float4 main(VertexShaderOutput input) : SV_TARGET0
{
    float4 color = gTexture.Sample(gSampler, input.texcoord);

    float2 correct = input.texcoord * (1.0f - input.texcoord.yx);
    float vignette = correct.x * correct.y * gVignetteParam.scale;
    vignette = saturate(pow(vignette, gVignetteParam.power));

    float factor = lerp(1.0f, vignette, saturate(gVignetteParam.intensity));
    color.rgb *= factor;
    return color;
}
