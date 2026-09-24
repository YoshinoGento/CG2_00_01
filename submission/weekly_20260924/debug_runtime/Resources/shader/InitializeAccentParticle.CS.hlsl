// AccentFX Particle Initialization Compute Shader
// FreeList initialization for AccentFX (max 512 particles)
// All particles are set to dead state and FreeList is filled sequentially

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

static const uint kMaxParticles = 512;

[numthreads(512, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint index = dispatchThreadId.x;
    if (index >= kMaxParticles)
    {
        return;
    }

    // All particles start dead
    Particle particle;
    particle.translate = float3(0.0f, 0.0f, 0.0f);
    particle.scale = float3(0.0f, 0.0f, 0.0f);
    particle.lifeTime = 0.0f;
    particle.velocity = float3(0.0f, 0.0f, 0.0f);
    particle.currentTime = 0.0f;
    particle.color = float4(0.0f, 0.0f, 0.0f, 0.0f);
    particle.isAlive = 0;
    gParticles[index] = particle;

    // Fill FreeList: index 0 has FreeListIndex = kMaxParticles - 1
    gFreeList[index] = index;

    // Thread 0 sets the FreeList counter to the last valid index
    if (index == 0)
    {
        gFreeListIndex[0] = (int)kMaxParticles - 1;
    }
}
