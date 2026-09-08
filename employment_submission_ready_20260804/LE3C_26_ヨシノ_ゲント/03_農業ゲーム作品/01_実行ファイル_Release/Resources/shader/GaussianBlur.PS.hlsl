Texture2D<float4> tex : register(t0);
SamplerState smp : register(s0);

cbuffer BlurParam : register(b0) {
    float2 texelSize; // ピクセル間の距離
    float padding[2];
    float4 weights0; // weights[0]~weights[3]
    float4 weights1; // weights[4]~weights[7]
};

struct VSOutput {
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};

float GetWeight(int index) {
    if (index < 4) {
        return weights0[index];
    } else {
        return weights1[index - 4];
    }
}

float4 main(VSOutput input) : SV_TARGET {
    // 中心ピクセルの色
    float4 color = tex.Sample(smp, input.uv) * GetWeight(0);
    
    // 周辺ピクセルの色をサンプリングして加算
    for (int i = 1; i < 8; ++i) {
        float2 offset = texelSize * float(i);
        float w = GetWeight(i);
        color += tex.Sample(smp, input.uv + offset) * w;
        color += tex.Sample(smp, input.uv - offset) * w;
    }
    
    return float4(color.rgb, 1.0f);
}
