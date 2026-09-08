#include "CopyImage.hlsli"

Texture2D<float4> gTexture : register(t0);
Texture2D<float4> gMaskTexture : register(t1);
SamplerState gSampler : register(s0);

struct DissolveParam
{
    float threshold;
    float edgeWidth;
    float edgeIntensity;
    float enableEdge;

    float3 edgeColor;
    float padding0;
};

ConstantBuffer<DissolveParam> gDissolveParam : register(b0);

float4 main(VertexShaderOutput input) : SV_TARGET0
{
    float4 color = gTexture.Sample(gSampler, input.texcoord);
    float mask = gMaskTexture.Sample(gSampler, input.texcoord).r;
    float threshold = saturate(gDissolveParam.threshold);

    if (mask <= threshold) {
        discard;
    }

    float edgeWidth = max(gDissolveParam.edgeWidth, 0.001f);
    float edgeFactor = 1.0f - smoothstep(threshold, threshold + edgeWidth, mask);
    edgeFactor *= saturate(gDissolveParam.enableEdge);

    color.rgb += gDissolveParam.edgeColor * edgeFactor * max(gDissolveParam.edgeIntensity, 0.0f);
    return float4(color.rgb, color.a);
}
