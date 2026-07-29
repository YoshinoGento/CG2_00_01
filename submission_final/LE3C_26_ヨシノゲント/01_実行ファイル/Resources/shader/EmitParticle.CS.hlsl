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

static const uint kMaxParticles = 1024;
static const uint kWaterSplashPreset = 1;

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
        int freeListIndex = -1;
        InterlockedAdd(gFreeListIndex[0], -1, freeListIndex);
        if (freeListIndex < 0 || freeListIndex >= (int)kMaxParticles)
        {
            int rollbackIndex = 0;
            InterlockedAdd(gFreeListIndex[0], 1, rollbackIndex);
            continue;
        }

        uint particleIndex = gFreeList[(uint)freeListIndex];
        if (particleIndex >= kMaxParticles)
        {
            int rollbackIndex = 0;
            InterlockedAdd(gFreeListIndex[0], 1, rollbackIndex);
            continue;
        }

        float t = ((float)i + 0.5f) / max((float)emitCount, 1.0f);
        float angle = (float)i * 2.39996323f;
        float3 offset;
        float3 velocity;
        if (gEmitter.preset == kWaterSplashPreset)
        {
            float3 forward = gEmitter.baseVelocity;
            float forwardLengthSquared = dot(forward, forward);
            forward = forwardLengthSquared > 1.0e-6f
                ? forward * rsqrt(forwardLengthSquared)
                : float3(0.0f, -1.0f, 0.0f);
            float3 referenceAxis = abs(forward.y) < 0.95f
                ? float3(0.0f, 1.0f, 0.0f)
                : float3(1.0f, 0.0f, 0.0f);
            float3 right = normalize(cross(referenceAxis, forward));
            float3 up = cross(forward, right);
            float3 radialDirection = right * cos(angle) + up * sin(angle);
            offset = radialDirection * (sqrt(t) * gEmitter.radius);
            float3 streamDirection = normalize(forward + radialDirection * 0.14f);
            velocity = streamDirection * gEmitter.speed * lerp(0.85f, 1.15f, t);
        }
        else
        {
            float y = 1.0f - 2.0f * t;
            float radial = sqrt(max(1.0f - y * y, 0.0f));
            float radiusScale = pow(t, 1.0f / 3.0f);
            float3 direction = float3(cos(angle) * radial, y, sin(angle) * radial);
            offset = direction * gEmitter.radius * radiusScale;
            velocity = gEmitter.baseVelocity + direction * gEmitter.speed;
        }

        Particle particle;
        particle.translate = gEmitter.translate + offset;
        particle.scale = gEmitter.scale;
        particle.lifeTime = gEmitter.lifeTime;
        particle.velocity = velocity;
        particle.currentTime = 0.0f;
        particle.color = gEmitter.color;
        particle.isAlive = 1;

        gParticles[particleIndex] = particle;
    }
}
