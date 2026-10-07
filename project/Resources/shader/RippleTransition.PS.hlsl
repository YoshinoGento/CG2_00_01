#include "Sprite2D.hlsli"
cbuffer TransitionConstants : register(b0) {
    float2 center;
    float progress;
    float revealing;
};
float4 main(VertexShaderOutput input) : SV_Target0 {
    const float2 resolution = float2(1280,720);
    const float3 water = float3(.012,.028,.035);
    // Exact endpoints guarantee no one-frame reveal when scene resources are replaced.
    if (progress <= 0) return float4(water,revealing);
    if (progress >= 1) return float4(water,1-revealing);
    float2 farthest = max(center,1-center)*resolution;
    float maxRadius = length(farthest)+32;
    float radius = lerp(-32,maxRadius,progress);
    float distance = length((input.texcoord-center)*resolution)-radius;
    float inside = 1-smoothstep(-3,3,distance);
    float primary = exp(-pow(distance/10,2));
    float echo = .38*exp(-pow((distance+22)/7,2));
    float crest = saturate(primary+echo);
    float alpha = revealing > .5 ? 1-inside : inside;
    float3 color = lerp(water,float3(.07,.24,.27),crest);
    return float4(color,saturate(alpha+crest*.35));
}
