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
RWStructuredBuffer<int> gFreeListIndex : register(u1);
RWStructuredBuffer<uint> gFreeList : register(u2);

static const uint kMaxParticles = 1024;

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

    bool shouldDie = particle.lifeTime <= 0.0f;

    if (!shouldDie)
    {
        float scaledDeltaTime = deltaTime * timeScale;
        particle.currentTime += scaledDeltaTime;
        particle.translate += particle.velocity * scaledDeltaTime;
        shouldDie = particle.currentTime >= particle.lifeTime;
    }

    if (shouldDie)
    {
        particle.isAlive = 0;
        particle.color.a = 0.0f;
        gParticles[index] = particle;

        int oldFreeListIndex = -1;
        InterlockedAdd(gFreeListIndex[0], 1, oldFreeListIndex);
        int pushIndex = oldFreeListIndex + 1;
        if (pushIndex >= 0 && pushIndex < (int)kMaxParticles)
        {
            gFreeList[(uint)pushIndex] = index;
        }
        else
        {
            int rollbackIndex = 0;
            InterlockedAdd(gFreeListIndex[0], -1, rollbackIndex);
        }
        return;
    }

    gParticles[index] = particle;
}
