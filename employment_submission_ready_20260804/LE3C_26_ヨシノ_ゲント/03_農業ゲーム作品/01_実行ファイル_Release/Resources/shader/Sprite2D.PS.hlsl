#include "Sprite2D.hlsli"

struct Material
{
    float4 color;
    int enableLighting; // スプライトでは使わないが、パディング合わせで残しておくのが安全
    float3 padding;
    float4x4 uvTransform;
};

// --- 古いシェーダーモデルでも動く cbuffer の書き方に変更 ---
cbuffer cbMaterial : register(b0)
{
    Material gMaterial;
};
// --------------------------------------------------------

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float4 color : SV_Target0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    
    // UV変換 (必要なら)
    float4 transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    
    // テクスチャサンプリング
    float4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
    
    // マテリアル色とテクスチャ色を合成
    output.color = gMaterial.color * textureColor;
    
    // アルファテスト: 完全に透明なピクセルは描画しない
    if (output.color.a == 0.0f)
    {
        discard;
    }
    
    return output;
}