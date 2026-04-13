#include "Object3d.hlsli"

// 定数バッファの登録
cbuffer cbMaterial : register(b0)
{
    Material gMaterial;
};
cbuffer cbDirectionalLight : register(b1)
{
    DirectionalLight gDirectionalLight;
};
cbuffer cbCamera : register(b2)
{
    Camera gCamera;
}; // ★追加

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
    
    if (gMaterial.enableLighting != 0)
    {
        // --- 準備 ---
        float3 N = normalize(input.normal); // 法線
        float3 L = normalize(-gDirectionalLight.direction); // ライトへの方向
        float3 V = normalize(gCamera.worldPosition - input.worldPosition); // 視線方向
        float3 H = normalize(L + V); // ハーフベクトル
        
        // --- 拡散反射 (Half-Lambert) ---
        float NdotL = dot(N, L);
        float cos = pow(NdotL * 0.5f + 0.5f, 2.0f);
        float3 diffuse = gMaterial.color.rgb * textureColor.rgb * gDirectionalLight.color.rgb * cos * gDirectionalLight.intensity;
        
        // --- 鏡面反射 (Blinn-Phong) ---
        // 法線とハーフベクトルの角度が近いほど光る
        float specularPower = pow(saturate(dot(N, H)), gMaterial.shininess);
        float3 specular = gDirectionalLight.color.rgb * gDirectionalLight.intensity * specularPower;
        
        // 最終合成（鏡面反射はテクスチャ色に依存せず、ライトの色で光るのが一般的）
        output.color.rgb = diffuse + specular;
        output.color.a = gMaterial.color.a * textureColor.a;
    }
    else
    {
        output.color = gMaterial.color * textureColor;
    }
    
    return output;
}