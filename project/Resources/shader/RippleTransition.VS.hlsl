#include "Sprite2D.hlsli"
VertexShaderOutput main(uint vertex : SV_VertexID) {
    VertexShaderOutput output;
    float2 uv = float2((vertex << 1) & 2,vertex & 2);
    output.position = float4(uv.x*2-1,1-uv.y*2,0,1);
    output.texcoord = uv;
    return output;
}
