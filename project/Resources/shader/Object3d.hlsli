struct VertexShaderOutput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
    float3 worldPosition : POSITION0; // ★追加：反射計算用
};

struct Material
{
    float4 color;
    int enableLighting;
    float shininess; // ★追加：光沢の強さ
    float4x4 uvTransform;
};

struct DirectionalLight
{
    float4 color;
    float3 direction;
    float intensity;
};

// ★追加：カメラ情報を渡すための構造体
struct Camera
{
    float3 worldPosition;
};