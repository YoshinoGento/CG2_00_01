cbuffer ReflectionConstants : register(b0) {
    float4x4 world, viewProjection;
    float4 tint, lightDirectionIntensity, lightColor;
    float4 settings; // water height, lit, reserved
    float4 outdoorAmbient;
};
struct ReflectionVertex {
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    float3 normal : NORMAL0;
    float aboveWater : SV_ClipDistance0;
};
