#include "Particle.hlsli"

struct Particle
{
    float3 translate;
    float3 scale;
    float lifeTime;
    float3 velocity;
    float currentTime;
    float4 color;
    uint isAlive;
	float3 acceleration;
	float3 startScale;
	float3 endScale;
	float startAlpha;
	float endAlpha;
	float drag;
	uint fadeMode;
};

struct ViewData
{
    float4x4 viewProjection;
    float4x4 billboardMatrix;
};

StructuredBuffer<Particle> gParticles : register(t0);

cbuffer cbViewData : register(b0)
{
    ViewData gViewData;
};

struct VertexShaderInput
{
    float4 position : POSITION;
    float3 normal : NORMAL;
    float2 texcoord : TEXCOORD;
    uint instanceId : SV_InstanceID;
};

VertexShaderOutput main(VertexShaderInput input)
{
    Particle particle = gParticles[input.instanceId];

    if (particle.isAlive == 0)
    {
        VertexShaderOutput deadOutput;
        deadOutput.position = float4(2.0f, 2.0f, 0.0f, 1.0f);
        deadOutput.texcoord = input.texcoord;
        deadOutput.color = float4(0.0f, 0.0f, 0.0f, 0.0f);
        return deadOutput;
    }

    float4 localPosition = input.position;
    localPosition.xyz *= particle.scale;

    float4 worldPosition = mul(localPosition, gViewData.billboardMatrix);
    worldPosition.xyz += particle.translate;
    worldPosition.w = 1.0f;

    VertexShaderOutput output;
    output.position = mul(worldPosition, gViewData.viewProjection);
    output.texcoord = input.texcoord;
    output.color = particle.color;
    return output;
}
