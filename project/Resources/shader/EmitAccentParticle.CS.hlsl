// AccentFX Particle Emit Compute Shader
// Emits particles with accent colors (white / yellow / cyan)
// Uses spherical distribution for even spread

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

struct GPUParticleEmitSettings
{
    float3 translate;
    float radius;

    float4 color;

    float3 scale;
    float lifeTime;

    float3 baseVelocity;
    float speed;

    uint count;
    uint emit;
    uint preset;
    uint padding;
};

RWStructuredBuffer<Particle> gParticles : register(u0);
RWStructuredBuffer<int> gFreeListIndex : register(u1);
RWStructuredBuffer<uint> gFreeList : register(u2);
ConstantBuffer<GPUParticleEmitSettings> gEmitter : register(b0);

static const uint kMaxParticles = 512;

[numthreads(256, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    if (gEmitter.emit == 0)
    {
        return;
    }

    uint i = dispatchThreadId.x;
    uint emitCount = min(gEmitter.count, kMaxParticles);
    if (i >= emitCount)
    {
        return;
    }

    // FreeList from top (stack pop)
    int freeListIndex = -1;
    InterlockedAdd(gFreeListIndex[0], -1, freeListIndex);
    if (freeListIndex < 0 || freeListIndex >= (int) kMaxParticles)
    {
        int rollbackIndex = 0;
        InterlockedAdd(gFreeListIndex[0], 1, rollbackIndex);
        return;
    }

    uint particleIndex = gFreeList[(uint) freeListIndex];
    if (particleIndex >= kMaxParticles)
    {
        int rollbackIndex = 0;
        InterlockedAdd(gFreeListIndex[0], 1, rollbackIndex);
        return;
    }

    // Spherical distribution using golden angle
    float t = ((float) i + 0.5f) / max((float) emitCount, 1.0f);
    float angle = (float) i * 2.39996323f; // golden angle
    float y = 1.0f - 2.0f * t;
    float radial = sqrt(max(1.0f - y * y, 0.0f));
    float radiusScale = pow(t, 1.0f / 3.0f);
    float3 direction = float3(cos(angle) * radial, y, sin(angle) * radial);
    float3 offset = direction * gEmitter.radius * radiusScale;

    Particle particle;
    particle.translate = gEmitter.translate + offset;
    particle.scale = gEmitter.scale;
    particle.lifeTime = gEmitter.lifeTime;
    particle.velocity = gEmitter.baseVelocity + direction * gEmitter.speed;
    particle.currentTime = 0.0f;
    particle.isAlive = 1;

    // AccentFX color palette: white / yellow / cyan
    // preset == 0: use gEmitter.color directly
    // preset == 1: accent color cycle
    if (gEmitter.preset == 1)
    {
        uint colorPattern = i % 4;
        if (colorPattern == 0)
        {
            // Bright white
            particle.color = float4(1.0f, 1.0f, 1.0f, 1.0f);
        }
        else if (colorPattern == 1)
        {
            // Warm yellow
            particle.color = float4(1.0f, 0.92f, 0.45f, 1.0f);
        }
        else if (colorPattern == 2)
        {
            // Cyan
            particle.color = float4(0.45f, 0.95f, 1.0f, 1.0f);
        }
        else
        {
            // Pale gold
            particle.color = float4(1.0f, 0.85f, 0.55f, 1.0f);
        }
    }
    else
    {
        particle.color = gEmitter.color;
    }

    gParticles[particleIndex] = particle;
}
