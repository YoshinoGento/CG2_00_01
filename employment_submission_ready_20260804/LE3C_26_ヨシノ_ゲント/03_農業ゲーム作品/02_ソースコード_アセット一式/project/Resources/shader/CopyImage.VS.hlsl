#include "CopyImage.hlsli"

VertexShaderOutput main(uint32_t vertexId : SV_VertexID)
{
    const float4 positions[3] = {
        float4(-1.0f,  1.0f, 0.0f, 1.0f),
        float4( 3.0f,  1.0f, 0.0f, 1.0f),
        float4(-1.0f, -3.0f, 0.0f, 1.0f),
    };
    const float2 texcoords[3] = {
        float2(0.0f, 0.0f),
        float2(2.0f, 0.0f),
        float2(0.0f, 2.0f),
    };

    VertexShaderOutput output;
    output.position = positions[vertexId];
    output.texcoord = texcoords[vertexId];
    return output;
}
