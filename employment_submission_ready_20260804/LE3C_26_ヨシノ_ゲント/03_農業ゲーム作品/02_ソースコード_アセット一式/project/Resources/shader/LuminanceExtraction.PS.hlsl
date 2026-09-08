Texture2D<float4> tex : register(t0);
SamplerState smp : register(s0);

cbuffer BloomParam : register(b0) {
    float threshold;
    float intensity;
    float padding1;
    float padding2;
};

struct VSOutput {
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};

float4 main(VSOutput input) : SV_TARGET {
    float4 color = tex.Sample(smp, input.uv);
    // ピクセルの輝度(Luminance)を計算
    float luminance = dot(color.rgb, float3(0.299f, 0.587f, 0.114f));
    // しきい値を超えた部分を抽出する
    // smoothstepを使って境界を少し滑らかにする
    float extract = smoothstep(threshold, threshold + 0.2f, luminance);
    
    // 抽出された部分に強度を掛けて返す
    return float4(color.rgb * extract * intensity, 1.0f);
}
