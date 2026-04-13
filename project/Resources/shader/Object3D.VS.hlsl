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
    
    // 座標変換 (スクリーン空間)
    output.position = mul(input.position, gTransformationMatrix.WVP);
    
    // ★追加：座標変換 (ワールド空間) - ピクセルシェーダーでの反射計算に使用
    output.worldPosition = mul(input.position, gTransformationMatrix.World).xyz;
    
    output.texcoord = input.texcoord;
    
    // 法線の変換（ワールド行列の回転成分を適用）
    output.normal = normalize(mul(input.normal, (float3x3) gTransformationMatrix.World));
    
    return output;
}