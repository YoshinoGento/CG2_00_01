#include "Object3d.hlsli"

struct TransformationMatrix
{
    float4x4 WVP;
    float4x4 World;
};

cbuffer cbTransformationMatrix : register(b0)
{
    TransformationMatrix gTransformationMatrix;
};

struct MatrixPalette
{
    float4x4 skeletonSpaceMatrix;
    float4x4 skeletonSpaceInverseTransposeMatrix;
};

StructuredBuffer<MatrixPalette> gMatrixPalette : register(t2);

struct VertexShaderInput
{
    float4 position : POSITION0;
    float3 normal : NORMAL0;
    float2 texcoord : TEXCOORD0;
    float4 weight : WEIGHT0;
    int4 index : INDEX0;
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;

    float4 skinnedPosition = float4(0.0f, 0.0f, 0.0f, 0.0f);
    float3 skinnedNormal = float3(0.0f, 0.0f, 0.0f);

    [unroll]
    for (int influenceIndex = 0; influenceIndex < 4; ++influenceIndex)
    {
        const float weight = input.weight[influenceIndex];
        const int jointIndex = input.index[influenceIndex];
        skinnedPosition += mul(input.position, gMatrixPalette[jointIndex].skeletonSpaceMatrix) * weight;
        skinnedNormal += mul(input.normal, (float3x3) gMatrixPalette[jointIndex].skeletonSpaceInverseTransposeMatrix) * weight;
    }

    skinnedPosition.w = 1.0f;
    const float normalLength = length(skinnedNormal);
    skinnedNormal = normalLength > 0.00001f ? normalize(skinnedNormal) : normalize(input.normal);

    output.position = mul(skinnedPosition, gTransformationMatrix.WVP);
    output.worldPosition = mul(skinnedPosition, gTransformationMatrix.World).xyz;
    output.texcoord = input.texcoord;
    output.normal = normalize(mul(skinnedNormal, (float3x3) gTransformationMatrix.World));

    return output;
}
