#include "Particle.hlsli"

// 頂点入力 (Slot 0)
struct VertexInput
{
    float4 position : POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};

// インスタンス入力 (Slot 1)
struct InstanceInput
{
    float4x4 wvp : WVP;
    float4x4 world : WORLD;
    float4 color : COLOR;
};

VertexShaderOutput main(VertexInput input, InstanceInput instance)
{
    VertexShaderOutput output;
    
    // 座標変換 (インスタンスごとのWVP行列を使用)
    // CPU側でBillboard計算済みのWorld行列を合成してWVPを作っている想定
    output.position = mul(input.position, instance.wvp);
    
    output.texcoord = input.texcoord;
    output.normal = normalize(mul(input.normal, (float3x3) instance.world));
    output.color = instance.color; // インスタンス色
    
    return output;
}