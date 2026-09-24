Texture2D<float4> tex : register(t0);
SamplerState smp : register(s0);

struct VSOutput
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};

float4 main(VSOutput input) : SV_TARGET
{
    // 元のゲーム画面の色を取得
    float4 color = tex.Sample(smp, input.uv);
    
    // ここで色を加工する（テストとして、少し青みがかったセピア調にする処理）
    // ※本格的なブルームやブラーにする場合はここを書き換えていきます
    float gray = color.r * 0.299 + color.g * 0.587 + color.b * 0.114;
    color.r = gray * 0.9;
    color.g = gray * 0.9;
    color.b = gray * 1.1;
    
    return color;
}