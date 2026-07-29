// LineVS.hlsl
#include "Line.hlsli"

struct TransformationMatrix {
    float4x4 WVP;
    float4x4 World;
};
cbuffer cbTransformationMatrix : register(b0) {
    TransformationMatrix gTransformationMatrix;
};

struct VertexShaderInput {
    float3 position : POSITION;
    float4 color : COLOR;
};

VertexShaderOutput main(VertexShaderInput input) {
    VertexShaderOutput output;
    output.position = mul(float4(input.position, 1.0f), gTransformationMatrix.WVP);
    output.color = input.color;
    return output;
}
