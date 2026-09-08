// SinglePassBloom.PS.hlsl
// 1パスで「高輝度抽出 + ぼかし + 合成」をすべて行うブルームシェーダー
// テクスチャの周辺ピクセルをサンプリングし、閾値以上の明るい部分だけを
// ガウス重み付きで集めて加算合成する

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

cbuffer BloomParam : register(b0) {
    float threshold;  // この輝度以上が光る
    float intensity;  // 光の強さ
    float blurRadius; // ぼかし半径（ピクセル数）
    float padding;
};

struct VSOutput {
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};

float4 main(VSOutput input) : SV_TARGET {
    float4 original = gTexture.Sample(gSampler, input.uv);
    float2 texelSize = float2(1.0 / 1280.0, 1.0 / 720.0);
    
    // 周辺の明るいピクセルを集めてブルーム（光の滲み）を生成する
    float3 bloom = float3(0, 0, 0);
    float totalWeight = 0.0001; // ゼロ除算防止
    
    // radiusの範囲内でサンプリング（最大6x6=13x13=169サンプル）
    int radius = clamp((int)blurRadius, 1, 6);
    float sigma = max(blurRadius * 0.5, 0.5);
    
    for (int y = -radius; y <= radius; y++) {
        for (int x = -radius; x <= radius; x++) {
            float2 offset = float2(x, y) * texelSize * 2.0;
            float4 s = gTexture.Sample(gSampler, input.uv + offset);
            
            // ピクセルの輝度を計算
            float lum = dot(s.rgb, float3(0.299, 0.587, 0.114));
            
            // 閾値を超えた明るさの部分だけを抽出
            float bright = smoothstep(threshold, threshold + 0.15, lum);
            
            // 距離に応じたガウス重み
            float dist = length(float2(x, y));
            float gaussWeight = exp(-dist * dist / (2.0 * sigma * sigma));
            
            float weight = bright * gaussWeight;
            bloom += s.rgb * weight;
            totalWeight += weight;
        }
    }
    
    bloom /= totalWeight;
    
    // 元画像にブルームを加算合成
    float3 finalColor = original.rgb + bloom * intensity;
    return float4(saturate(finalColor), 1.0);
}
