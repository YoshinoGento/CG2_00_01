struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
    float3 worldPosition : POSITION0; // ★追加: 鏡面反射の計算に使用
};

struct Material
{
    float4 color;
    int enableLighting;
    float shininess; // ★追加: 輝きの鋭さ
    float4x4 uvTransform;
};

struct DirectionalLight
{
    float4 color;
    float3 direction;
    float intensity;
};

struct Camera
{
    float3 worldPosition; // ★追加: カメラの座標
};