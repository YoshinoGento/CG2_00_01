#include "TitleGrass.hlsli"
cbuffer ShadowScene : register(b1) {
    float4x4 lightViewProjection;
    float2 shadowTexelSize;
    float depthBias;
    float normalBias;
    float shadowStrength;
    float3 padding;
};
Texture2D<float> shadowMap : register(t0);
SamplerComparisonState shadowSampler : register(s0);
struct PixelOutput {
    float4 color : SV_TARGET0;
#ifndef TITLE_REFLECTION
    float4 normal : SV_TARGET1;
#endif
};
float Visibility(float3 position, float3 normal, float3 direction) {
    if (shadowStrength <= 0) return 1;
    float4 clip = mul(float4(position,1),lightViewProjection);
    if (clip.w <= 0) return 1;
    float3 ndc = clip.xyz/clip.w;
    if (any(abs(ndc.xy) > 1) || ndc.z <= 0 || ndc.z >= 1) return 1;
    float2 uv = float2(ndc.x*.5+.5,-ndc.y*.5+.5);
    float depth = ndc.z-max(depthBias,normalBias*(1-saturate(dot(normal,direction))));
    float visibility = 0;
    [unroll] for (int y=-1; y<=1; ++y) [unroll] for (int x=-1; x<=1; ++x)
        visibility += shadowMap.SampleCmpLevelZero(shadowSampler,uv+float2(x,y)*shadowTexelSize,depth);
    return lerp(1,visibility/9,saturate(shadowStrength));
}
PixelOutput main(GrassOutput input, bool front : SV_IsFrontFace) {
    PixelOutput output;
    float3 normal = normalize(input.normal)*(front ? 1 : -1);
    float3 direction = normalize(-lightDirectionIntensity.xyz);
    float shade = min(3,floor(saturate(input.shade)*4))/3;
    float3 pigment = lerp(float3(.11,.22,.055),float3(.30,.44,.14),shade)*input.tint.rgb;
    float visibility = Visibility(input.worldPosition,normal,direction);
    // Thin opaque leaves receive backlighting too; alpha/sorting are not involved.
    // Diffuse sky fill dominates: horizon ray changes cannot amplify ground-compensated light.
    float illumination = .90+.10*abs(dot(normal,direction))*visibility;
    float waveTint = .80+.20*input.wave;
    output.color = float4(pigment*lightColor.rgb*lightDirectionIntensity.w*illumination*waveTint,1);
#ifndef TITLE_REFLECTION
    output.normal = float4(normal*.5+.5,1);
#endif
    return output;
}
