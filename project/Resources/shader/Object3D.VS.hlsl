#include "Object3d.hlsli"

struct TransformationMatrix
{
    float32_t4x4 WVP;
    float32_t4x4 World;
};

ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);

struct VertexShaderInput
{
    float32_t4 position : POSITION0;
    float32_t2 texcoord : TEXCOORD0;
    float32_t3 normal : NORMAL0;
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    
    // 座標変換
    output.position = mul(input.position, gTransformationMatrix.WVP);
    
    // テクスチャ座標
    output.texcoord = input.texcoord;
    
    // 法線の変換
    // World行列を使って法線も回転させる（平行移動成分は無視するため 3x3 にキャスト）
    output.normal = normalize(mul(input.normal, (float32_t3x3) gTransformationMatrix.World));
    
    return output;
}