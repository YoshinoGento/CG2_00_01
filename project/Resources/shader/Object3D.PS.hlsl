#include "Object3d.hlsli"

// 定数バッファのバインド
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
        float3 N = normalize(input.normal); // 面の向き
        float3 L = normalize(-gDirectionalLight.direction); // 光の向き (逆向きにする)
        float3 V = normalize(gCamera.worldPosition - input.worldPosition); // 視線の向き
        float3 H = normalize(L + V); // ハーフベクトル (LとVの中間)
        
        // --- 拡散反射 (Half-Lambert) ---
        float NdotL = dot(N, L);
        float cos = pow(NdotL * 0.5f + 0.5f, 2.0f);
        float3 diffuse = gMaterial.color.rgb * textureColor.rgb * gDirectionalLight.color.rgb * cos * gDirectionalLight.intensity;
        
        // --- 鏡面反射 (Blinn-Phong) ---
        // 法線NとハーフベクトルHが一致するほど光る
        float specularFactor = pow(saturate(dot(N, H)), gMaterial.shininess);
        float3 specular = gDirectionalLight.color.rgb * gDirectionalLight.intensity * specularFactor;
        
        // 合計 (鏡面反射はハイライトなのでテクスチャの色を掛けない)
        output.color.rgb = diffuse + specular;
        output.color.a = gMaterial.color.a * textureColor.a;
    }
    else
    {
        output.color = gMaterial.color * textureColor;
    }
    
    return output;
}