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
	uint extendedSettings;

	float3 direction;
	float directionSpread;

	float3 acceleration;
	float drag;

	float3 endScale;
	float endAlpha;

	float4 colorVariance;

	float lifeTimeVariance;
	float speedVariance;
	float scaleVariance;
	float innerRadius;

	uint shape;
	uint randomSeed;
	uint fadeMode;
	uint padding;
};

RWStructuredBuffer<Particle> gParticles : register(u0);
RWStructuredBuffer<int> gFreeListIndex : register(u1);
RWStructuredBuffer<uint> gFreeList : register(u2);
ConstantBuffer<GPUParticleEmitSettings> gEmitter : register(b0);

static const uint kMaxParticles = 1024;

float Random01(uint value)
{
	value ^= value >> 16;
	value *= 0x7feb352du;
	value ^= value >> 15;
	value *= 0x846ca68bu;
	value ^= value >> 16;
	return (float)(value & 0x00ffffffu) / 16777215.0f;
}

float RandomSigned(uint value)
{
	return Random01(value) * 2.0f - 1.0f;
}

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
		uint seed = i * 747796405u + gEmitter.randomSeed * 2891336453u;
		float angle = Random01(seed + 1u) * 6.28318530718f;
		float y = 1.0f - 2.0f * Random01(seed + 2u);
        float radial = sqrt(max(1.0f - y * y, 0.0f));
		float radiusRandom = Random01(seed + 3u);
		float radiusMinimum = min(gEmitter.innerRadius, gEmitter.radius);
		float radiusScale = lerp(radiusMinimum, gEmitter.radius, pow(radiusRandom, 1.0f / 3.0f));
        float3 direction = float3(cos(angle) * radial, y, sin(angle) * radial);
		if (gEmitter.shape == 1u)
		{
			direction.y = abs(direction.y);
		}
		else if (gEmitter.shape == 2u)
		{
			float3 axis = normalize(length(gEmitter.direction) > 0.0001f ? gEmitter.direction : float3(0.0f, 1.0f, 0.0f));
			float3 helper = abs(axis.y) < 0.99f ? float3(0.0f, 1.0f, 0.0f) : float3(1.0f, 0.0f, 0.0f);
			float3 tangent = normalize(cross(helper, axis));
			float3 bitangent = cross(axis, tangent);
			float coneAngle = Random01(seed + 4u) * gEmitter.directionSpread;
			direction = normalize(axis * cos(coneAngle) +
				(tangent * cos(angle) + bitangent * sin(angle)) * sin(coneAngle));
		}
		else if (gEmitter.shape == 3u)
		{
			direction = float3(cos(angle), 0.0f, sin(angle));
		}
		float3 offset = direction * radiusScale;
		float lifeTime = max(0.05f, gEmitter.lifeTime + RandomSigned(seed + 5u) * gEmitter.lifeTimeVariance);
		float speed = max(0.0f, gEmitter.speed + RandomSigned(seed + 6u) * gEmitter.speedVariance);
		float scaleFactor = max(0.01f, 1.0f + RandomSigned(seed + 7u) * gEmitter.scaleVariance);
		float4 color = saturate(gEmitter.color + float4(
			RandomSigned(seed + 8u) * gEmitter.colorVariance.r,
			RandomSigned(seed + 9u) * gEmitter.colorVariance.g,
			RandomSigned(seed + 10u) * gEmitter.colorVariance.b,
			0.0f));

        Particle particle;
        particle.translate = gEmitter.translate + offset;
		particle.startScale = gEmitter.scale * scaleFactor;
		particle.endScale = gEmitter.endScale * scaleFactor;
		particle.scale = particle.startScale;
		particle.lifeTime = lifeTime;
		particle.velocity = gEmitter.baseVelocity + direction * speed;
        particle.currentTime = 0.0f;
		particle.color = color;
		particle.startAlpha = color.a;
		particle.endAlpha = saturate(gEmitter.endAlpha);
		particle.acceleration = gEmitter.acceleration;
		particle.drag = max(gEmitter.drag, 0.0f);
		particle.fadeMode = min(gEmitter.fadeMode, 2u);
        particle.isAlive = 1;

        gParticles[particleIndex] = particle;
    }
}
