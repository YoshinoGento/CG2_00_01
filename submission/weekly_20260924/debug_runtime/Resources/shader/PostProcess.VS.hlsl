struct VSOutput
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
};

// 頂点バッファを使わずに、画面全体を覆う巨大な三角形を1枚描画する特殊な手法
VSOutput main(uint id : SV_VertexID)
{
    VSOutput output;
    output.uv = float2((id << 1) & 2, id & 2);
    output.pos = float4(output.uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    return output;
}