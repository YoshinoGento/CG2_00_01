// AccentFX Particle Update Compute Shader
// Crop Burst phases: Absorb -> Impact -> Burst -> Fade.
// Distance checks use squared length. Speed clamp uses dot + rsqrt.

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

static const uint PHASE_NONE = 0;
static const uint PHASE_ABSORB = 1;
static const uint PHASE_IMPACT = 2;
static const uint PHASE_BURST = 3;
static const uint PHASE_FADE = 4;

RWStructuredBuffer<Particle> gParticles : register(u0);
RWStructuredBuffer<int> gFreeListIndex : register(u1);
RWStructuredBuffer<uint> gFreeList : register(u2);

cbuffer CropBurstInfo : register(b0)
{
    float3 center;
    float radiusSq;
    float strength;
    uint phase;
    float phaseTime;
    float deltaTime;
    float maxSpeed;
    uint particleCount;
    float timeScale;
    uint padding;
};

void ClampSpeed(inout float3 velocity, float maxSpeedValue)
{
    float maxSpeedSq = maxSpeedValue * maxSpeedValue;
    float speedSq = dot(velocity, velocity);
    if (speedSq > maxSpeedSq && speedSq > 0.0001f)
    {
        velocity *= maxSpeedValue * rsqrt(speedSq);
    }
}

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

    float scaledDeltaTime = deltaTime * timeScale;
    particle.currentTime += scaledDeltaTime;

    bool shouldDie = particle.lifeTime > 0.0f && particle.currentTime >= particle.lifeTime;

    if (!shouldDie)
    {
        float3 toCenter = center - particle.translate;
        float distSq = dot(toCenter, toCenter);

        switch (phase)
        {
        case PHASE_ABSORB:
        {
            if (distSq < radiusSq && distSq > 0.0001f)
            {
                float3 dir = toCenter * rsqrt(distSq);
                float pullStrength = strength * (0.8f + phaseTime * 3.0f);
                particle.velocity += dir * pullStrength * scaledDeltaTime;
            }

            float absorbAlpha = saturate(0.55f + phaseTime * 0.45f);
            particle.color.a = max(particle.color.a, absorbAlpha);
            particle.scale *= max(1.0f - phaseTime * 0.25f, 0.55f);
            break;
        }
        case PHASE_IMPACT:
        {
            particle.velocity *= 0.15f;
            float flashT = saturate(phaseTime * 8.0f);
            particle.color.rgb = lerp(particle.color.rgb, float3(1.8f, 1.8f, 1.8f), flashT);
            particle.color.a = 1.0f;
            if (distSq > 0.01f)
            {
                particle.translate += toCenter * rsqrt(distSq) * 0.7f * scaledDeltaTime;
            }
            particle.scale = max(particle.scale, float3(0.28f, 0.28f, 0.28f));
            break;
        }
        case PHASE_BURST:
        {
            float3 outDir;
            if (distSq > 0.0001f)
            {
                outDir = -toCenter * rsqrt(distSq);
            }
            else
            {
                float angle = (float) index * 2.39996323f;
                outDir = float3(cos(angle), 0.0f, sin(angle));
            }

            outDir.y += 0.65f;
            outDir = normalize(outDir);
            float burstPower = strength * 2.8f * max(1.0f - phaseTime * 0.7f, 0.35f);
            particle.velocity += outDir * burstPower * scaledDeltaTime;
            particle.velocity.y -= 1.0f * scaledDeltaTime;
            particle.color.a = saturate(1.0f - phaseTime * 0.25f);
            break;
        }
        case PHASE_FADE:
        {
            particle.velocity *= max(1.0f - 1.8f * scaledDeltaTime, 0.0f);
            particle.velocity.y -= 0.8f * scaledDeltaTime;
            float fadeAlpha = saturate(1.0f - phaseTime);
            particle.color.a = fadeAlpha;
            particle.scale *= max(1.0f - phaseTime * 0.12f, 0.88f);
            if (fadeAlpha <= 0.01f)
            {
                shouldDie = true;
            }
            break;
        }
        default:
        {
            float lifeRatio = particle.currentTime / max(particle.lifeTime, 0.001f);
            particle.color.a = saturate(1.0f - lifeRatio);
            particle.velocity.y -= 0.5f * scaledDeltaTime;
            break;
        }
        }

        ClampSpeed(particle.velocity, maxSpeed);
        particle.translate += particle.velocity * scaledDeltaTime;
    }

    if (shouldDie)
    {
        particle.isAlive = 0;
        particle.color.a = 0.0f;
        gParticles[index] = particle;

        int oldFreeListIndex = -1;
        InterlockedAdd(gFreeListIndex[0], 1, oldFreeListIndex);
        int pushIndex = oldFreeListIndex + 1;
        if (pushIndex >= 0 && pushIndex < (int) particleCount)
        {
            gFreeList[(uint) pushIndex] = index;
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
