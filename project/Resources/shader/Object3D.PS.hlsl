#include "Object3d.hlsli"

// 定数バッファのバインド (RootSignature の index と register を一致させる)
cbuffer cbMaterial : register(b0) // Index 0
{
    Material gMaterial;
};

cbuffer cbDirectionalLight : register(b1) // Index 2 (C++側で2番に設定)
{
    DirectionalLight gDirectionalLight;
};

cbuffer cbCamera : register(b2) // Index 3
{
    Camera gCamera;
};

// ★追加：スポットライト (Index 4)
struct SpotLight
{
    float4 color;
    float3 position;
    float intensity;
    float3 direction;
    float distance;
    float decay;
    float cosAngle;
    float cosFalloffStart;
    float padding[2];
};
cbuffer cbSpotLight : register(b3)
{
    SpotLight gSpotLight;
};

Texture2D<float4> gTexture : register(t0); // Index 5
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    float4 textureColor = gTexture.Sample(gSampler, input.texcoord);
    
    // ライティング無効ならテクスチャ色をそのまま出す
    if (gMaterial.enableLighting == 0)
    {
        output.color = gMaterial.color * textureColor;
        return output;
    }

    // --- 準備 ---
    float3 N = normalize(input.normal);
    float3 V = normalize(gCamera.worldPosition - input.worldPosition);

    // --- 1. 平行光源 ---
    float3 L_dir = normalize(-gDirectionalLight.direction);
    float3 H_dir = normalize(L_dir + V);
    float NdotL_dir = saturate(dot(N, L_dir));
    float cos_dir = pow(NdotL_dir * 0.5f + 0.5f, 2.0f);
    float3 diffuse_dir = gMaterial.color.rgb * textureColor.rgb * gDirectionalLight.color.rgb * cos_dir * gDirectionalLight.intensity;
    float spec_dir = pow(saturate(dot(N, H_dir)), gMaterial.shininess);
    float3 specular_dir = gDirectionalLight.color.rgb * gDirectionalLight.intensity * spec_dir;

    // --- 2. スポットライト ---
    float3 lightVec = input.worldPosition - gSpotLight.position;
    float dist = length(lightVec);
    float3 L_spot = normalize(-lightVec);
    
    // 距離減衰
    float distAtten = pow(saturate(1.0f - dist / gSpotLight.distance), gSpotLight.decay);
    // 角度減衰
    float cosTheta = dot(normalize(lightVec), gSpotLight.direction);
    float angleAtten = saturate((cosTheta - gSpotLight.cosAngle) / (gSpotLight.cosFalloffStart - gSpotLight.cosAngle));
    
    float spotFactor = gSpotLight.intensity * distAtten * angleAtten;
    float3 H_spot = normalize(L_spot + V);
    float3 diffuse_spot = gMaterial.color.rgb * textureColor.rgb * gSpotLight.color.rgb * saturate(dot(N, L_spot)) * spotFactor;
    float3 specular_spot = gSpotLight.color.rgb * spotFactor * pow(saturate(dot(N, H_spot)), gMaterial.shininess);

    // 合成
    output.color.rgb = diffuse_dir + specular_dir + diffuse_spot + specular_spot;
    output.color.a = gMaterial.color.a * textureColor.a;

    return output;
}