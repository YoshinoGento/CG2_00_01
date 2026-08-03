#include "Particle.hlsli"

struct TransformationMatrix
{
    float4x4 WVP;
    float4x4 World;
    float4 color;
};

struct VertexShaderInput
{
    float4 position : POSITION;
    float2 texcoord : TEXCOORD;
    float3 normal : NORMAL;
    float4x4 instanceWVP : WVP;
    float4x4 instanceWorld : WORLD;
    float4 instanceColor : COLOR;
    float4 uvTransform : TEXCOORD1;
};

struct VSOutput
{
    float4 pos : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float4 color : COLOR;
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;

    output.position = mul(input.position, input.instanceWVP);
    
    output.texcoord = input.texcoord * input.uvTransform.xy + input.uvTransform.zw;
    
    output.color = input.instanceColor;
    return output;
}