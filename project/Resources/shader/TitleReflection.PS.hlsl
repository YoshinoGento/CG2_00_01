#include "TitleReflection.hlsli"
Texture2D<float4> materialImage : register(t0);
SamplerState materialSampler : register(s0);
float4 main(ReflectionVertex input) : SV_Target0 {
    float3 color = materialImage.Sample(materialSampler,input.uv).rgb*tint.rgb;
    float diffuse = saturate(dot(normalize(input.normal),-lightDirectionIntensity.xyz));
    float3 lighting = lightColor.rgb*lightDirectionIntensity.w*(.18+.82*diffuse);
    return float4(color*lerp(float3(1,1,1),lighting,settings.y),1);
}
