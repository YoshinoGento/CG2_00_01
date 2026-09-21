#include "Skybox.hlsli"

struct VertexShaderInput
{
    float4 position : POSITION0;
};

VertexShaderOutput main(VertexShaderInput input)
{
    VertexShaderOutput output;
    
    // WVP行列で変換
    output.position = mul(input.position, gTransformationMatrix.WVP);
    
    /**
     * 資料スライド 13: z を w に置き換える魔法
     * これにより、どれだけ近くに描画しようとしても、GPUは「これは一番奥にある」と判断します。
     * .xyww と書くことで、z成分をwの値で上書きしています。
     */
    output.position = output.position.xyww;
    
    // スライド 11: 箱の頂点座標をそのまま方向ベクトルとして利用する
    output.texcoord = input.position.xyz;
    
    return output;
}