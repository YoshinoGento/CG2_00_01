#include "CopyImage.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct RandomNoiseParam
{
    float time;
    float strength;
    float scale;
    float mode;

    float animate;
    float padding0;
    float padding1;
    float padding2;
};

ConstantBuffer<RandomNoiseParam> gRandomNoiseParam : register(b0);

float rand2dTo1d(float2 seed)
{
    return frac(sin(dot(seed, float2(12.9898f, 78.233f))) * 43758.5453f);
}

float4 main(VertexShaderOutput input) : SV_TARGET0
{
    float4 sceneColor = gTexture.Sample(gSampler, input.texcoord);

    float timeOffset = gRandomNoiseParam.animate > 0.5f ? gRandomNoiseParam.time : 0.0f;
    float2 seed = input.texcoord * max(gRandomNoiseParam.scale, 1.0f) + float2(timeOffset, timeOffset * 0.37f);
    float random = rand2dTo1d(seed);

    if (gRandomNoiseParam.mode < 0.5f)
    {
        return float4(random, random, random, sceneColor.a);
    }

    float noiseFactor = lerp(1.0f, random, saturate(gRandomNoiseParam.strength));
    return float4(sceneColor.rgb * noiseFactor, sceneColor.a);
}
