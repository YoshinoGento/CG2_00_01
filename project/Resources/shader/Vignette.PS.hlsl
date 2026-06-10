#include "CopyImage.hlsli"

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct VignetteParam
{
    float32_t scale;
    float32_t power;
    float32_t intensity;
    float32_t padding;
};

ConstantBuffer<VignetteParam> gVignetteParam : register(b0);

float32_t4 main(VertexShaderOutput input) : SV_TARGET0
{
    float32_t4 color = gTexture.Sample(gSampler, input.texcoord);

    float32_t2 correct = input.texcoord * (1.0f - input.texcoord.yx);
    float32_t vignette = correct.x * correct.y * gVignetteParam.scale;
    vignette = saturate(pow(vignette, gVignetteParam.power));

    float32_t factor = lerp(1.0f, vignette, saturate(gVignetteParam.intensity));
    color.rgb *= factor;
    return color;
}
