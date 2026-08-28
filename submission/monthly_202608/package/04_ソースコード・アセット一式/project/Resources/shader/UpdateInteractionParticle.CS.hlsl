struct Particle
{
    float3 translate;
    float3 scale;
    float lifeTime;
    float3 velocity;
    float currentTime;
    float4 color;
    uint isAlive;
};

struct GPUParticleInteractionSettings
{
    float3 gridCenter;
    float particleSize;

    uint gridCountX;
    uint gridCountY;
    uint gridCountZ;
    uint particleCount;

    float3 brushPosition;
    float brushRadius;

    float brushStrength;
    uint isPressed;
    uint operation;
    float deltaTime;

    float damping;
    float padding0;
    float padding1;
    float padding2;
};

RWStructuredBuffer<Particle> gParticles : register(u0);
ConstantBuffer<GPUParticleInteractionSettings> gInteraction : register(b0);

[numthreads(256, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint index = dispatchThreadId.x;
    if (index >= gInteraction.particleCount)
    {
        return;
    }

    Particle particle = gParticles[index];
    if (particle.isAlive == 0)
    {
        return;
    }

    float dt = max(gInteraction.deltaTime, 0.0f);

    if (gInteraction.isPressed != 0 &&
        gInteraction.operation != 0 &&
        gInteraction.operation <= 2)
    {
        float3 toParticle = particle.translate - gInteraction.brushPosition;
        float brushRadius = max(gInteraction.brushRadius, 0.0001f);
        float radiusSq = brushRadius * brushRadius;
        float distanceSq = dot(toParticle, toParticle);
        if (distanceSq > 0.0001f && distanceSq < radiusSq)
        {
            float invDistance = rsqrt(distanceSq);
            float distanceRatioSq = saturate(distanceSq / radiusSq);
            float falloff = 1.0f - distanceRatioSq;
            float3 pushDirection = toParticle * invDistance;
            float directionSign = gInteraction.operation == 2 ? -1.0f : 1.0f;
            particle.velocity += pushDirection * directionSign * gInteraction.brushStrength * falloff * dt;
        }
    }

    particle.translate += particle.velocity * dt;

    float damping = saturate(gInteraction.damping);
    particle.velocity *= pow(damping, dt * 60.0f);

    gParticles[index] = particle;
}
