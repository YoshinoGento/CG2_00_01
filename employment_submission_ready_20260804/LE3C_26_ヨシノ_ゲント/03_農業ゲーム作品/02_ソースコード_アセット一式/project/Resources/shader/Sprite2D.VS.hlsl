#include "Sprite2D.hlsli"

struct TransformationMatrix
{
    float4x4 WVP;
    float4x4 World;
};

// --- 古いシェーダーモデルでも動く cbuffer の書き方に変更 ---
cbuffer cbTransformationMatrix : register(b0)
{
    TransformationMatrix gTransformationMatrix;
};
// --------------------------------------------------------

struct VertexShaderInput
{
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    
    // 座標変換
    output.position = mul(input.position, gTransformationMatrix.WVP);
    output.texcoord = input.texcoord;
    
    return output;
}