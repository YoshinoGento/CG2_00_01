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

RWStructuredBuffer<Particle> gParticles : register(u0);

cbuffer UpdateParticleInfo : register(b0)
{
    float deltaTime;
    uint particleCount;
    float timeScale;
    uint padding;
};

[numthreads(256, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint index = dispatchThreadId.x;
    if (index >= particleCount)
    {
        return;
    }

    Particle particle = gParticles[index];
    if (particle.isAlive == 0)
    {
        return;
    }

    if (particle.lifeTime <= 0.0f)
    {
        particle.isAlive = 0;
        particle.color.a = 0.0f;
        gParticles[index] = particle;
        return;
    }

    float scaledDeltaTime = deltaTime * timeScale;
    particle.currentTime += scaledDeltaTime;
    particle.translate += particle.velocity * scaledDeltaTime;

    if (particle.currentTime >= particle.lifeTime)
    {
        particle.isAlive = 0;
        particle.color.a = 0.0f;
    }

    gParticles[index] = particle;
}
