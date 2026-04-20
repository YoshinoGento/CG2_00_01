#include "Particle.hlsli"

struct TransformationMatrix
{
    float4x4 WVP;
    float4x4 World;
    float4 color;
};

// インスタンシングデータを受け取る構造
struct VertexShaderInput
{
    float4 position : POSITION;
    float2 texcoord : TEXCOORD;
    float3 normal : NORMAL;
    // インスタンシングバッファから受け取るデータ
    float4x4 instanceWVP : WVP;
    float4x4 instanceWorld : WORLD;
    float4 instanceColor : COLOR;
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    // インスタンスごとの行列で変換
    output.position = mul(input.position, input.instanceWVP);
    output.texcoord = input.texcoord;
    output.color = input.instanceColor;
    return output;
}