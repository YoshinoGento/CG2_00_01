#include "Particle.hlsli"

// === 定数バッファ（CBuffer） ===
// CPU側の ParticleManager::MaterialData に対応
// Root Parameter 1 → register(b0) にバインドされる
cbuffer cbMaterial : register(b0)
{
    float gAlphaReference; // discardしきい値（この値以下のαを持つピクセルは棄却）
    float3 padding;        // 16バイトアラインメントのためのパディング
};

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

    // 色の計算
    // ブレンド設定が SrcAlpha / One（加算合成）なので、シンプルな乗算
    output.color.rgb = textureColor.rgb * input.color.rgb;

    // 画像そのもののAlphaが0なら、輝度(R)をアルファとして代用するハイブリッド方式
    float alpha = max(textureColor.a, textureColor.r);
    output.color.a = alpha * input.color.a;

    // === discardのしきい値判定（資料スライド10の実装） ===
    // CBufferから受け取った gAlphaReference と比較
    // この値以下のアルファを持つピクセルは描画をスキップし、背景を保護する
    //
    // 使い方の例:
    //   gAlphaReference = 0.0  → 完全に透明なものだけ棄却（従来動作）
    //   gAlphaReference = 0.5  → 半透明以下を全て棄却（くっきりした輪郭）
    //   gAlphaReference = 0.0  → しきい値なし（全ピクセル描画）
    if (output.color.a <= gAlphaReference)
    {
        discard;
    }

    // 輝度を少しブースト
    output.color.rgb *= 1.2f;
    output.color = saturate(output.color);
    
    return output;
}