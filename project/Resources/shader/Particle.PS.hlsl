#include "Particle.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    
    float4 textureColor = gTexture.Sample(gSampler, input.texcoord);

    // 円形マスク（中心を (0.5,0.5) として距離でマスク）
    float2 uv = input.texcoord;
    float2 d = uv - float2(0.5, 0.5);
    float dist = length(d);
    const float radius = 0.5; // 半径（UV空間）
    const float edge = 0.02; // エッジ幅（ソフトマスク）
    // mask: 1.0 内側、0.0 外側、edge でフェード
    float mask = 1.0 - smoothstep(radius - edge, radius, dist);

    // テクスチャカラーのアルファにマスクを掛ける
    textureColor.a *= mask;

    // テクスチャカラー * インスタンスカラー (頂点色)
    output.color = textureColor * input.color;
    
    return output;
}