#include "CopyImage.hlsli"

Texture2D<float32_t4> gTexture : register(t0);
SamplerState gSampler : register(s0);

// 定数バッファ (標準偏差を受け取る)
cbuffer GaussianParam : register(b0)
{
    float32_t sigma;
};

static const float32_t PI = 3.14159265f;

// ---------------------------------------------------------
// ガウス関数の重みを計算する関数
// x, y: 中心ピクセルからのインデックス距離
// sigma: 標準偏差
// ---------------------------------------------------------
float32_t CalculateGaussianWeight(float32_t x, float32_t y, float32_t s)
{
    float32_t sigma2 = s * s;
    
    // G(x,y)の前半の係数: (1 / (2 * PI * sigma^2))
    float32_t coefficient = rcp(2.0f * PI * sigma2);
    
    // G(x,y)の指数部分: -(x^2 + y^2) / (2 * sigma^2)
    float32_t exponent = -(x * x + y * y) * rcp(2.0f * sigma2);
    
    // exp() を使用してネイピア数eの累乗を計算し、最終的な重みを返す
    return coefficient * exp(exponent);
}

struct PixelShaderOutput
{
    float32_t4 color : SV_TARGET0;
};

// ---------------------------------------------------------
// ピクセルシェーダー メイン関数
// 3x3のガウシアンフィルターによる畳み込み処理
// ---------------------------------------------------------
PixelShaderOutput main(VertexShaderOutput input)
{
    PixelShaderOutput output;

    // テクスチャのサイズを取得してピクセル間のUV距離を算出
    uint32_t width;
    uint32_t height;
    gTexture.GetDimensions(width, height);
    float32_t2 uvStepSize = float32_t2(rcp(float32_t(width)), rcp(float32_t(height)));

    // 加算用の初期化
    float32_t3 result = float32_t3(0.0f, 0.0f, 0.0f);
    float32_t alpha = 0.0f;
    float32_t totalWeight = 0.0f;
    
    // 3x3カーネルの走査 (-1 から 1 まで)
    [unroll]
    for (int32_t y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int32_t x = -1; x <= 1; ++x)
        {
            // サンプリング対象のUV座標を算出
            float32_t2 offset = float32_t2(float32_t(x), float32_t(y)) * uvStepSize;
            float32_t2 sampleUV = input.texcoord + offset;
            
            // 対象ピクセルの色をサンプリング
            float32_t4 sampleColor = gTexture.Sample(gSampler, sampleUV);
            
            // 関数の引数としてインデックスとsigmaを渡し、該当位置のガウス重みを計算
            float32_t weight = CalculateGaussianWeight(float32_t(x), float32_t(y), sigma);
            
            // 色に対して重みを乗算し、結果に加算
            result += sampleColor.rgb * weight;
            alpha += sampleColor.a * weight;
            
            // 正規化のために重みの合計を蓄積
            totalWeight += weight;
        }
    }
    
    // 【重要】有限範囲による重みの欠損を補正するための正規化
    // 合計重みの逆数を乗算することで、画像全体が暗くなるのを防ぎ底上げを行う
    result *= rcp(totalWeight);
    alpha *= rcp(totalWeight);
    
    output.color = float32_t4(result, alpha);
    return output;
}
