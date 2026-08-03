#include "Skybox.hlsli"

/**
 * 資料スライド 14: キューブマップ用サンプリング
 * Texture2D ではなく TextureCube を使います。
 */
TextureCube<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

float4 main(VertexShaderOutput input) : SV_TARGET
{
    // 3次元ベクトルでテクスチャから色を取得
    float4 textureColor = gTexture.Sample(gSampler, input.texcoord);
    
    // マテリアルの色を掛けて最終的な色を決定
    return textureColor * gMaterial.color;
}