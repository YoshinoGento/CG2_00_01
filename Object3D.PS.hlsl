#include "Object3d.hlsli"

struct Material
{
    float32_t4 color;
    int32_t enableLighting;
    float32_t4x4 uvTransform;
};

struct DirectionalLight
{
    float32_t4 color;
    float32_t3 direction;
    float intensity;
};

// カメラ座標
struct Camera
{
    float32_t3 worldPosition;
};

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

// 定数バッファ
ConstantBuffer<Material> gMaterial : register(b0);
ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);
ConstantBuffer<Camera> gCamera : register(b2); // カメラは b2

struct PixelShaderOutput
{
    float32_t4 color : SV_Target0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    
    // UV変換とテクスチャ色取得
    float4 transformedUV = mul(float32_t4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float32_t4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);

    // ライティング計算
    if (gMaterial.enableLighting != 0)
    {
        float32_t3 N = normalize(input.normal);
        // ライトの逆ベクトル（光が来る方向）
        float32_t3 L = normalize(-gDirectionalLight.direction);
        
        // ------------------------------------------------------------------
        // ★ 1. 拡散反射 (Half-Lambert / ハーフランバート)
        // ------------------------------------------------------------------
        float NdotL = dot(N, L);
        float cos = pow(NdotL * 0.5f + 0.5f, 2.0f); // 影を柔らかくする計算

        // ------------------------------------------------------------------
        // ★ 2. 鏡面反射 (Phong Specular / フォン反射)
        // ------------------------------------------------------------------
        float32_t3 toEye = normalize(gCamera.worldPosition - input.worldPosition);
        float32_t3 reflectVector = reflect(gDirectionalLight.direction, N);
        float RdotE = dot(reflectVector, toEye);
        float specularPow = pow(saturate(RdotE), 50.0f); // ツヤの鋭さ

        // ------------------------------------------------------------------
        // 合成
        // ------------------------------------------------------------------
        float32_t3 diffuse = gMaterial.color.rgb * textureColor.rgb * gDirectionalLight.color.rgb * cos * gDirectionalLight.intensity;
        float32_t3 specular = gDirectionalLight.color.rgb * gDirectionalLight.intensity * specularPow * float32_t3(1.0f, 1.0f, 1.0f);

        output.color.rgb = diffuse + specular;
        output.color.a = gMaterial.color.a * textureColor.a;
    }
    else
    {
        output.color = gMaterial.color * textureColor;
    }
    
    //if (output.color.a == 0.0)
    //{
    //    discard;
    //}

    return output;
}