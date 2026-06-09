#include "CopyImage.hlsli"

VertexShaderOutput main(uint32_t vertexId : SV_VertexID)
{
    const float32_t4 positions[3] = {
        float32_t4(-1.0f,  1.0f, 0.0f, 1.0f),
        float32_t4( 3.0f,  1.0f, 0.0f, 1.0f),
        float32_t4(-1.0f, -3.0f, 0.0f, 1.0f),
    };
    const float32_t2 texcoords[3] = {
        float32_t2(0.0f, 0.0f),
        float32_t2(2.0f, 0.0f),
        float32_t2(0.0f, 2.0f),
    };

    VertexShaderOutput output;
    output.position = positions[vertexId];
    output.texcoord = texcoords[vertexId];
    return output;
}
