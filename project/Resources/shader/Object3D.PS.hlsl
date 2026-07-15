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

struct ShadowScene
{
    float4x4 lightViewProjection;
    float2 texelSize;
    float depthBias;
    float normalBias;
    float strength;
    float3 padding;
};
cbuffer cbShadowScene : register(b4)
{
    ShadowScene gShadowScene;
};

Texture2D<float4> gTexture : register(t0); // Index 5
TextureCube<float4> gEnvironmentTexture : register(t1); // ★追加
Texture2D<float> gDirectionalShadowMap : register(t3);
SamplerState gSampler : register(s0);
SamplerComparisonState gShadowSampler : register(s1);

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
    float4 normal : SV_TARGET1;
};

float EvaluateDirectionalShadow(float3 worldPosition, float3 normal, float3 lightDirection)
{
    float4 lightClipPosition = mul(float4(worldPosition, 1.0f), gShadowScene.lightViewProjection);
    if (lightClipPosition.w <= 0.0f)
    {
        return 1.0f;
    }

    float3 lightNdc = lightClipPosition.xyz / lightClipPosition.w;
    if (lightNdc.x < -1.0f || lightNdc.x > 1.0f ||
        lightNdc.y < -1.0f || lightNdc.y > 1.0f ||
        lightNdc.z <= 0.0f || lightNdc.z >= 1.0f)
    {
        return 1.0f;
    }

    float2 shadowUv = float2(lightNdc.x * 0.5f + 0.5f, -lightNdc.y * 0.5f + 0.5f);
    float slopeBias = gShadowScene.normalBias * (1.0f - saturate(dot(normal, lightDirection)));
    float receiverDepth = lightNdc.z - max(gShadowScene.depthBias, slopeBias);
    float visibility = 0.0f;

    [unroll]
    for (int y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int x = -1; x <= 1; ++x)
        {
            float2 offset = float2((float)x, (float)y) * gShadowScene.texelSize;
            visibility += gDirectionalShadowMap.SampleCmpLevelZero(
                gShadowSampler, shadowUv + offset, receiverDepth);
        }
    }

    visibility /= 9.0f;
    return lerp(1.0f, visibility, saturate(gShadowScene.strength));
}

PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;
    float3 N = length(input.normal) > 0.00001f ? normalize(input.normal) : float3(0.0f, 1.0f, 0.0f);
    output.normal = float4(N * 0.5f + 0.5f, 1.0f);
    float4 textureColor = gTexture.Sample(gSampler, input.texcoord);
    
    // ライティング無効ならテクスチャ色をそのまま出す
    if (gMaterial.enableLighting == 0)
    {
        output.color = gMaterial.color * textureColor;
        return output;
    }

    // --- 準備 ---
    float3 V = normalize(gCamera.worldPosition - input.worldPosition);

    // --- 1. 平行光源 ---
    float3 L_dir = normalize(-gDirectionalLight.direction);
    float3 H_dir = normalize(L_dir + V);
    float NdotL_dir = saturate(dot(N, L_dir));
    float directionalShadow = EvaluateDirectionalShadow(input.worldPosition, N, L_dir);
    float3 baseColor = gMaterial.color.rgb * textureColor.rgb;
    float3 ambient_dir = baseColor * gDirectionalLight.color.rgb * 0.18f * gDirectionalLight.intensity;
    float3 diffuse_dir = baseColor * gDirectionalLight.color.rgb * (NdotL_dir * 0.82f) * gDirectionalLight.intensity * directionalShadow;
    float spec_dir = pow(saturate(dot(N, H_dir)), gMaterial.shininess);
    float3 specular_dir = gDirectionalLight.color.rgb * gDirectionalLight.intensity * spec_dir * directionalShadow;

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
    
     // --- 3. 環境マップ (★追加) ---
    // カメラから点への入射ベクトルを法線で反射させる
    float3 cameraToPosition = normalize(input.worldPosition - gCamera.worldPosition);
    float3 reflectedVector = reflect(cameraToPosition, N);
    // 反射した方向の景色をサンプリング
    float3 environmentColor = gEnvironmentTexture.Sample(gSampler, reflectedVector).rgb;


    // 全てを合成
    output.color.rgb = ambient_dir + diffuse_dir + specular_dir + diffuse_spot + specular_spot;
    // 環境マップの色を係数に従って加算
    output.color.rgb += environmentColor * gMaterial.environmentCoefficient;
    
    output.color.a = gMaterial.color.a * textureColor.a;

    return output;
}
