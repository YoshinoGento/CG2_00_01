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
    
    // テクスチャをサンプリング
    float4 textureColor = gTexture.Sample(gSampler, input.texcoord);

    // ★真相ポイント：色の計算
    // ブレンド設定を SrcAlpha / One にしたので、ここでは Alpha を RGB に掛けないシンプルな掛け算にします。
    output.color.rgb = textureColor.rgb * input.color.rgb;

    // 画像そのものの Alpha が 0 なら、輝度 (R) をアルファとして代用するハイブリッド方式
    float alpha = max(textureColor.a, textureColor.r);
    output.color.a = alpha * input.color.a;

    // 完全に透明なピクセルは計算をスキップし、背景を保護する
    if (output.color.a <= 0.0f)
    {
        discard;
    }

    // 輝度を少しブースト
    output.color.rgb *= 1.2f;
    output.color = saturate(output.color);
    
    return output;
}