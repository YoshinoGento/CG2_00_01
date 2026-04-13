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

struct VertexShaderInput
{
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    
    // WVP行列で座標変換
    output.position = mul(input.position, gTransformationMatrix.WVP);
    
    // ★追加: ワールド行列で座標変換（ピクセルシェーダーでの反射計算用）
    output.worldPosition = mul(input.position, gTransformationMatrix.World).xyz;
    
    output.texcoord = input.texcoord;
    
    // 法線の変換（回転のみ適用）
    output.normal = normalize(mul(input.normal, (float3x3) gTransformationMatrix.World));
    
    return output;
}