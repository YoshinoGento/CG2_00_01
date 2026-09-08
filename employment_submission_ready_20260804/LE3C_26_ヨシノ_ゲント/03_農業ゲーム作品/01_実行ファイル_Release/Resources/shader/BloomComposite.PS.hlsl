Texture2D<float4> tex0 : register(t0); // 元のゲーム画面
Texture2D<float4> tex1 : register(t1); // ブラーのかかった高輝度画面
SamplerState smp : register(s0);

struct VSOutput {
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};

float4 main(VSOutput input) : SV_TARGET {
    float4 color0 = tex0.Sample(smp, input.uv);
    float4 color1 = tex1.Sample(smp, input.uv);
    
    // 加算合成
    float3 finalColor = color0.rgb + color1.rgb;
    finalColor = saturate(finalColor);
    
    return float4(finalColor, 1.0f);
}
