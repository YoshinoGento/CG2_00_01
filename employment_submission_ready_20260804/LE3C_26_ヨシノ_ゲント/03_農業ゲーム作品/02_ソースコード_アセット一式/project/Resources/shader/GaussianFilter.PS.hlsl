#include "CopyImage.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

// 定数バッファ (標準偏差を受け取めE
cbuffer GaussianParam : register(b0)
{
    float sigma;
};

static const float PI = 3.14159265f;

// ---------------------------------------------------------
// ガウス関数の重みを計算する関数
// x, y: 中忁E��クセルからのインチE��クス距離
// sigma: 標準偏差
// ---------------------------------------------------------
float CalculateGaussianWeight(float x, float y, float s)
{
    float sigma2 = s * s;
    
    // G(x,y)の前半の係数: (1 / (2 * PI * sigma^2))
    float coefficient = rcp(2.0f * PI * sigma2);
    
    // G(x,y)の持E��部刁E -(x^2 + y^2) / (2 * sigma^2)
    float exponent = -(x * x + y * y) * rcp(2.0f * sigma2);
    
    // exp() を使用してネイピア数eの累乗を計算し、最終的な重みを返す
    return coefficient * exp(exponent);
}

struct PixelShaderOutput
{
    float4 color : SV_TARGET0;
};

// ---------------------------------------------------------
// ピクセルシェーダー メイン関数
// 3x3のガウシアンフィルターによる畳み込み処琁E
// ---------------------------------------------------------
PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    // チE��スチャのサイズを取得してピクセル間�EUV距離を算�E
    uint32_t width;
    uint32_t height;
    gTexture.GetDimensions(width, height);
    float2 uvStepSize = float2(rcp(float(width)), rcp(float(height)));

    // 加算用の初期匁E
    float3 result = float3(0.0f, 0.0f, 0.0f);
    float alpha = 0.0f;
    float totalWeight = 0.0f;
    
    // 3x3カーネルの走査 (-1 から 1 まで)
    [unroll]
    for (int32_t y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int32_t x = -1; x <= 1; ++x)
        {
            // サンプリング対象のUV座標を算�E
            float2 offset = float2(float(x), float(y)) * uvStepSize;
            float2 sampleUV = input.texcoord + offset;
            
            // 対象ピクセルの色をサンプリング
            float4 sampleColor = gTexture.Sample(gSampler, sampleUV);
            
            // 関数の引数としてインチE��クスとsigmaを渡し、該当位置のガウス重みを計箁E
            float weight = CalculateGaussianWeight(float(x), float(y), sigma);
            
            // 色に対して重みを乗算し、結果に加箁E
            result += sampleColor.rgb * weight;
            alpha += sampleColor.a * weight;
            
            // 正規化のために重みの合計を蓁E��E
            totalWeight += weight;
        }
    }
    
    // 【重要】有限篁E��による重みの欠損を補正するための正規化
    // 合計重みの送E��を乗算することで、画像�E体が暗くなる�Eを防ぎ底上げを行う
    result *= rcp(totalWeight);
    alpha *= rcp(totalWeight);
    
    output.color = float4(result, alpha);
    return output;
}
