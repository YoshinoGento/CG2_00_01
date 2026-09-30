cbuffer Sky : register(b0) {
    row_major float4x4 wvp;
    float4 topColor;
    float4 horizonColor;
    float4 cloudColor;
    float4 sun;
    float4 moon;
    float4 discs;
    float4 motion;
    float4 animation;
};
struct SkyVertex {
    float4 position : SV_POSITION;
    float3 direction : TEXCOORD0;
};
