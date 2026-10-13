cbuffer GrassScene : register(b0) {
    float4x4 viewProjection;
    float4 lightColor;
    float4 lightDirectionIntensity;
    float4 wind; // world X/Z slopes, signed traveling phase, inverse bend reference height
    float4 wave; // fixed cloud-motion axis X/Z, spatial frequency, local phase spread
};
struct GrassOutput {
    float4 position : SV_POSITION;
    float3 worldPosition : TEXCOORD0;
    float3 normal : NORMAL0;
    float4 tint : COLOR0;
    float shade : TEXCOORD1;
    float wave : TEXCOORD2;
};
