#include "Object3d.hlsli"

struct Material
{
    float32_t4 color;
    int32_t enableLighting;
    float32_t4x4 uvTransform;
};

struct DirectionalLight
{
    float32_t4 color; // ライトの色
    float32_t3 direction; // ライトの向き
    float intensity; // 輝度
};

// レジスタ設定 (Object3d.cppの設定と合わせる)
ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);
Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    
    // UV変換
    float4 transformedUV = mul(float32_t4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    
    // テクスチャサンプリング
    float32_t4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
    
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
        // マテリアル色 × テクスチャ色 × ライト色 × 明るさ(cos) × 輝度
        output.color = gMaterial.color * textureColor * gDirectionalLight.color * cos * gDirectionalLight.intensity;
    }
    else
    {
        // ライティング無効時（そのままの色）
        output.color = gMaterial.color * textureColor;
    }
    
    return output;
}