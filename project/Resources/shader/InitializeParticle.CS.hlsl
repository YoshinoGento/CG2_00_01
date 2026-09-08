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

[numthreads(1024, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint index = dispatchThreadId.x;
    if (index >= kMaxParticles)
    {
        return;
    }

    if (index == 0)
    {
        gFreeListIndex[0] = (int)kMaxParticles - 1;
    }

    Particle particle;
    particle.translate = float3(0.0f, 0.0f, 0.0f);

    // Debug option: spread particles on a small grid when visibility at the origin is hard to confirm.
    // uint x = index % 32;
    // uint y = index / 32;
    // particle.translate = float3(((float)x - 15.5f) * 0.15f, ((float)y - 15.5f) * 0.15f, 0.0f);

    particle.scale = float3(0.5f, 0.5f, 0.5f);
    particle.lifeTime = 1.0f;
    particle.velocity = float3(0.0f, 0.0f, 0.0f);
    particle.currentTime = 0.0f;
    particle.color = float4(1.0f, 1.0f, 1.0f, 1.0f);
    particle.isAlive = 0;
	particle.acceleration = float3(0.0f, 0.0f, 0.0f);
	particle.startScale = particle.scale;
	particle.endScale = particle.scale;
	particle.startAlpha = 1.0f;
	particle.endAlpha = 1.0f;
	particle.drag = 0.0f;
	particle.fadeMode = 0;

    gParticles[index] = particle;
    gFreeList[index] = index;
}
