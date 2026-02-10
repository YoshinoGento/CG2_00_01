#include "Sprite2D.hlsli"

struct TransformationMatrix
{
    float32_t4x4 WVP;
    float32_t4x4 World;
};

// ★修正: Sprite.cpp の SetGraphicsRootConstantBufferView(1, ...) に合わせるため
// register(b0) ではなく register(b1) にします。
// b0 は Material 用に使われています。
ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b1);

struct VertexShaderInput
{
    float32_t4 position : POSITION0;
    float32_t2 texcoord : TEXCOORD0;
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    
    // 座標変換
    output.position = mul(input.position, gTransformationMatrix.WVP);
    output.texcoord = input.texcoord;
    
    return output;
}