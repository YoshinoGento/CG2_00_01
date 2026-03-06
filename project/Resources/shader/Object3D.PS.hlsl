#include "Object3d.hlsli"

struct Material
{
    float4 color;
    int enableLighting;
    float4x4 uvTransform;
};

struct DirectionalLight
{
    float4 color; // ライトの色
    float3 direction; // ライトの向き
    float intensity; // 輝度
};

// --- 古いシェーダーモデルでも動く cbuffer の書き方に変更 ---
cbuffer cbMaterial : register(b0)
{
    Material gMaterial;
};

cbuffer cbDirectionalLight : register(b1)
{
    DirectionalLight gDirectionalLight;
};
// --------------------------------------------------------

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    
    // UV変換
    float4 transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    
    // テクスチャサンプリング
    float4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
    
    // ライティングの計算
    if (gMaterial.enableLighting != 0)
    {
        // --- Half-Lambert (ハーフランバート) ---
        // 法線とライト方向の内積（cosθ）を計算
        // ライトの向きは「降り注ぐ方向」なので反転させる(-1倍)
        float NdotL = dot(normalize(input.normal), -gDirectionalLight.direction);
        
        // 通常のLambertは 0.0 ~ 1.0 だが、
        // Half-Lambertは 0.5 ~ 1.0 の範囲に圧縮してから2乗する
        // これにより、光の当たらない部分も少し明るくなり、柔らかい印象になる
        float cos = pow(NdotL * 0.5f + 0.5f, 2.0f);
        
        // 最終的な色の計算
        output.color = gMaterial.color * textureColor * gDirectionalLight.color * cos * gDirectionalLight.intensity;
    }
    else
    {
        // ライティング無効時（そのままの色）
        output.color = gMaterial.color * textureColor;
    }
    
    return output;
}