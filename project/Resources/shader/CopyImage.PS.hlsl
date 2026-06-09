#include "CopyImage.hlsli"

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

float32_t4 main(VertexShaderOutput input) : SV_TARGET0
{
    return gTexture.Sample(gSampler, input.texcoord);
}
