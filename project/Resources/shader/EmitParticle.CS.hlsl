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

        float t = ((float)i + 0.5f) / max((float)emitCount, 1.0f);
        float angle = (float)i * 2.39996323f;
        float y = 1.0f - 2.0f * t;
        float radial = sqrt(max(1.0f - y * y, 0.0f));
        float radiusScale = pow(t, 1.0f / 3.0f);
        float3 direction = float3(cos(angle) * radial, y, sin(angle) * radial);
        float3 offset = direction * gEmitter.radius * radiusScale;

        Particle particle;
        // Debug-visible spawn: lift particles above the ground and make them distinct from white lighting.
        particle.translate = gEmitter.translate + offset + float3(0.0f, 1.5f, -2.0f);
        particle.scale = float3(0.75f, 0.75f, 0.75f);
        particle.lifeTime = 5.0f;
        particle.velocity = direction * 0.35f + float3(0.0f, 0.15f, 0.0f);
        particle.currentTime = 0.0f;
        particle.color = float4(1.0f, 0.0f, 1.0f, 1.0f);
        particle.isAlive = 1;

        gParticles[particleIndex] = particle;
    }
}
