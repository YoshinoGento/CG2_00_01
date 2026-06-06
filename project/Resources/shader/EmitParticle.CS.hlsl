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

struct EmitterSphere
{
    float3 translate;
    float radius;
    uint count;
    float frequency;
    float frequencyTime;
    uint emit;
};

RWStructuredBuffer<Particle> gParticles : register(u0);
RWStructuredBuffer<uint> gFreeCounter : register(u1);
ConstantBuffer<EmitterSphere> gEmitter : register(b0);

static const uint kMaxParticles = 1024;

[numthreads(1, 1, 1)]
void main()
{
    if (gEmitter.emit == 0)
    {
        return;
    }

    uint emitCount = min(gEmitter.count, kMaxParticles);
    for (uint i = 0; i < emitCount; ++i)
    {
        uint particleIndex = 0;
        InterlockedAdd(gFreeCounter[0], 1, particleIndex);
        if (particleIndex >= kMaxParticles)
        {
            continue;
        }

        float angle = (float)i * 2.39996323f;
        float normalizedRadius = sqrt(((float)i + 0.5f) / max((float)emitCount, 1.0f));
        float emitRadius = gEmitter.radius * normalizedRadius;
        float3 offset = float3(cos(angle) * emitRadius, sin(angle) * emitRadius, 0.0f);

        Particle particle;
        particle.translate = gEmitter.translate + offset;
        particle.scale = float3(0.25f, 0.25f, 0.25f);
        particle.lifeTime = 2.0f;
        particle.velocity = float3(offset.x * 0.5f, 0.35f, offset.y * 0.5f);
        particle.currentTime = 0.0f;
        particle.color = float4(1.0f, 1.0f, 1.0f, 1.0f);
        particle.isAlive = 1;

        gParticles[particleIndex] = particle;
    }
}
