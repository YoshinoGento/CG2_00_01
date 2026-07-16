#include "Object3d.hlsli"

struct TransformationMatrix
{
    float4x4 WVP;
    float4x4 World;
    float4x4 WorldInverseTranspose;
};

cbuffer cbTransformationMatrix : register(b0)
{
    TransformationMatrix gTransformationMatrix;
};

struct VertexShaderInput
{
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    
    // 座標変換 (スクリーン空間)
    output.position = mul(input.position, gTransformationMatrix.WVP);
    
    // ★追加：座標変換 (ワールド空間) - ピクセルシェーダーでの反射計算に使用
    output.worldPosition = mul(input.position, gTransformationMatrix.World).xyz;
    
    output.texcoord = input.texcoord;
    
    // Positions and normals require different transforms when scale is non-uniform.
    float3 worldNormal = mul(input.normal, (float3x3) gTransformationMatrix.WorldInverseTranspose);
    float worldNormalLengthSquared = dot(worldNormal, worldNormal);
    output.normal = worldNormalLengthSquared > 1.0e-10f
        ? worldNormal * rsqrt(worldNormalLengthSquared)
        : float3(0.0f, 1.0f, 0.0f);
    
    return output;
}
