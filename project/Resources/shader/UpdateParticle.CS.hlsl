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
		particle.velocity += particle.acceleration * scaledDeltaTime;
		particle.velocity *= exp(-particle.drag * scaledDeltaTime);
        particle.translate += particle.velocity * scaledDeltaTime;
		float normalizedTime = saturate(particle.currentTime / max(particle.lifeTime, 0.0001f));
		float fadeTime = normalizedTime;
		if (particle.fadeMode == 1u)
		{
			fadeTime = 1.0f - (1.0f - normalizedTime) * (1.0f - normalizedTime);
		}
		else if (particle.fadeMode == 2u)
		{
			fadeTime = smoothstep(0.0f, 1.0f, normalizedTime);
		}
		particle.scale = lerp(particle.startScale, particle.endScale, normalizedTime);
		particle.color.a = lerp(particle.startAlpha, particle.endAlpha, fadeTime);
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
