#include "Object3d.hlsli"

// マテリアル (b0)
cbuffer cbMaterial : register(b0)
{
    Material gMaterial;
};

// 平行光源 (b1)
cbuffer cbDirectionalLight : register(b1)
{
    DirectionalLight gDirectionalLight;
};

// カメラ (b2)
cbuffer cbCamera : register(b2)
{
    Camera gCamera;
};

// ★追加：スポットライト (b3)
struct SpotLight
{
    float4 color; // ライトの色
    float3 position; // ライトの位置
    float intensity; // 輝度
    float3 direction; // ライトの向き
    float distance; // ライトの届く最大距離
    float decay; // 減衰率 (距離)
    float cosAngle; // スポットライトの最大角度の余弦 (cos)
    float cosFalloffStart; // 減衰し始める角度の余弦 (cos)
    float padding[2];
};

cbuffer cbSpotLight : register(b3)
{
    SpotLight gSpotLight;
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
    float4 textureColor = gTexture.Sample(gSampler, input.texcoord);
    
    // ライティングが無効ならテクスチャの色をそのまま返す
    if (gMaterial.enableLighting == 0)
    {
        output.color = gMaterial.color * textureColor;
        return output;
    }

    // --- 共通の準備 ---
    float3 N = normalize(input.normal); // 面の法線
    float3 V = normalize(gCamera.worldPosition - input.worldPosition); // 視線ベクトル

    // --- 1. 平行光源の計算 (既存) ---
    float3 L_dir = normalize(-gDirectionalLight.direction);
    float3 H_dir = normalize(L_dir + V);
    float NdotL_dir = saturate(dot(N, L_dir));
    float cos_dir = pow(NdotL_dir * 0.5f + 0.5f, 2.0f); // ハーフランバート
    float3 diffuse_dir = gMaterial.color.rgb * textureColor.rgb * gDirectionalLight.color.rgb * cos_dir * gDirectionalLight.intensity;
    float specFactor_dir = pow(saturate(dot(N, H_dir)), gMaterial.shininess);
    float3 specular_dir = gDirectionalLight.color.rgb * gDirectionalLight.intensity * specFactor_dir;

    // --- 2. スポットライトの計算 (★追加) ---
    // ライトからピクセルへのベクトル
    float3 lightVec = input.worldPosition - gSpotLight.position;
    float dist = length(lightVec);
    float3 L_spot = normalize(-lightVec); // ライトへ向かうベクトル
    
    // 距離減衰 (離れるほど暗く)
    float distanceAttenuation = pow(saturate(1.0f - dist / gSpotLight.distance), gSpotLight.decay);
    
    // 角度減衰 (中心軸から外れるほど暗く)
    // 頂点方向とライトの向きの内積で角度(cos)を求める
    float cosTheta = dot(normalize(lightVec), gSpotLight.direction);
    // 指示のあった数式：(cosθ - cosAngle) / (cosFalloffStart - cosAngle) でスムーズな輪郭を作る
    float angleAttenuation = saturate((cosTheta - gSpotLight.cosAngle) / (gSpotLight.cosFalloffStart - gSpotLight.cosAngle));
    
    float totalSpotIntensity = gSpotLight.intensity * distanceAttenuation * angleAttenuation;
    
    // スポットライト用の拡散反射・鏡面反射
    float3 H_spot = normalize(L_spot + V);
    float NdotL_spot = saturate(dot(N, L_spot));
    float3 diffuse_spot = gMaterial.color.rgb * textureColor.rgb * gSpotLight.color.rgb * NdotL_spot * totalSpotIntensity;
    float specFactor_spot = pow(saturate(dot(N, H_spot)), gMaterial.shininess);
    float3 specular_spot = gSpotLight.color.rgb * totalSpotIntensity * specFactor_spot;

    // --- 最終色の合成 ---
    // 平行光源とスポットライトを足し合わせる
    output.color.rgb = diffuse_dir + specular_dir + diffuse_spot + specular_spot;
    output.color.a = gMaterial.color.a * textureColor.a;

    return output;
}