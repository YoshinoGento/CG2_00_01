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
    
    // テクスチャから色をサンプリング
    float4 textureColor = gTexture.Sample(gSampler, input.texcoord);

    // ★修正ポイント：テクスチャの色を「光の強さ」として扱う
    // 加算合成（Additive）の場合、色が (0,0,0) なら何も描画されず、(1,1,1) なら白く光ります。
    // 元の画像が「黒背景に白い円」なら、これで正しく光る部分だけが残ります。
    output.color = textureColor * input.color;
    
    return output;
}