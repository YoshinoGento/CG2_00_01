// 頂点シェーダーからピクセルシェーダーへの橋渡し構造体
struct VertexShaderOutput
{
    float4 position : SV_POSITION; // 画面上の座標 (x, y, z, w)
    float3 texcoord : TEXCOORD0; // 3次元の方向ベクトル
};

// 変換行列の構造定義
struct TransformationMatrix
{
    float4x4 WVP;
    float4x4 World;
};

// マテリアルの構造定義
struct Material
{
    float4 color;
};

// --- ここでエラーが起きていた箇所を修正 ---

// ConstantBuffer<TransformationMatrix> ではなく、cbuffer ブロックを使う
cbuffer cbTransformationMatrix : register(b0)
{
    TransformationMatrix gTransformationMatrix;
};

// ConstantBuffer<Material> ではなく、cbuffer ブロックを使う
cbuffer cbMaterial : register(b1)
{
    Material gMaterial;
};