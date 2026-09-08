struct VertexData
{
    float4 position;
    float3 normal;
    float2 texcoord;
};

struct VertexInfluence
{
    float weights[4];
    int jointIndices[4];
};

struct MatrixPalette
{
    float4x4 skeletonSpaceMatrix;
    float4x4 skeletonSpaceInverseTransposeMatrix;
};

cbuffer SkinningInfo : register(b0)
{
    uint vertexCount;
    uint jointCount;
};

StructuredBuffer<VertexData> gInputVertices : register(t0);
StructuredBuffer<VertexInfluence> gInfluences : register(t1);
StructuredBuffer<MatrixPalette> gMatrixPalette : register(t2);
RWStructuredBuffer<VertexData> gOutputVertices : register(u0);

[numthreads(64, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint vertexIndex = dispatchThreadId.x;
    if (vertexIndex >= vertexCount)
    {
        return;
    }

    VertexData input = gInputVertices[vertexIndex];
    VertexInfluence influence = gInfluences[vertexIndex];

    float4 skinnedPosition = float4(0.0f, 0.0f, 0.0f, 0.0f);
    float3 skinnedNormal = float3(0.0f, 0.0f, 0.0f);
    bool hasValidInfluence = false;

    [unroll]
    for (int influenceIndex = 0; influenceIndex < 4; ++influenceIndex)
    {
        float weight = influence.weights[influenceIndex];
        int jointIndex = influence.jointIndices[influenceIndex];
        if (weight == 0.0f || jointIndex < 0 || jointIndex >= jointCount)
        {
            continue;
        }

        hasValidInfluence = true;
        skinnedPosition += mul(input.position, gMatrixPalette[jointIndex].skeletonSpaceMatrix) * weight;
        skinnedNormal += mul(input.normal, (float3x3)gMatrixPalette[jointIndex].skeletonSpaceInverseTransposeMatrix) * weight;
    }

    float normalLength = length(skinnedNormal);
    VertexData output;
    output.position = hasValidInfluence ? float4(skinnedPosition.xyz, 1.0f) : input.position;
    output.normal = normalLength > 0.00001f ? normalize(skinnedNormal) : input.normal;
    output.texcoord = input.texcoord;

    gOutputVertices[vertexIndex] = output;
}
