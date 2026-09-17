// テスト用：入力画像の色相を反転する（動作確認シェーダー）
Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct VSOutput {
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};

float4 main(VSOutput input) : SV_TARGET {
    float4 color = gTexture.Sample(gSampler, input.uv);
    // 色を反転させる（元画像が正しく届いていれば、画面の色が反転するはず）
    return float4(1.0f - color.rgb, 1.0f);
}
