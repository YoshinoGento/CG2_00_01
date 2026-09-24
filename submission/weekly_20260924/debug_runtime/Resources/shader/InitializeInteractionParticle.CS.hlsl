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

struct GPUParticleInteractionSettings
{
    float3 gridCenter;
    float particleSize;

    uint gridCountX;
    uint gridCountY;
    uint gridCountZ;
    uint particleCount;

    float3 brushPosition;
    float brushRadius;

    float brushStrength;
    uint isPressed;
    uint operation;
    float deltaTime;

    float damping;
    float padding0;
    float padding1;
    float padding2;
};

RWStructuredBuffer<Particle> gParticles : register(u0);
ConstantBuffer<GPUParticleInteractionSettings> gInteraction : register(b0);

static const uint kMaxParticles = 1024;

[numthreads(256, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint index = dispatchThreadId.x;
    if (index >= kMaxParticles)
    {
        return;
    }

    Particle particle;
    particle.translate = float3(0.0f, 0.0f, 0.0f);
    particle.scale = float3(gInteraction.particleSize, gInteraction.particleSize, gInteraction.particleSize);
    particle.lifeTime = 999999.0f;
    particle.velocity = float3(0.0f, 0.0f, 0.0f);
    particle.currentTime = 0.0f;
    particle.color = float4(0.86f, 0.76f, 0.52f, 1.0f);
    particle.isAlive = 0;

    uint gridCountX = max(gInteraction.gridCountX, 1u);
    uint gridCountY = max(gInteraction.gridCountY, 1u);
    uint gridCountZ = max(gInteraction.gridCountZ, 1u);
    uint particleCount = min(gInteraction.particleCount, kMaxParticles);

    if (index < particleCount)
    {
        uint xyCount = gridCountX * gridCountY;
        uint x = index % gridCountX;
        uint y = (index / gridCountX) % gridCountY;
        uint z = index / xyCount;

        float spacing = max(gInteraction.particleSize * 4.0f, 0.08f);
        float3 halfExtent = float3(
            (float)(gridCountX - 1) * 0.5f,
            (float)(gridCountY - 1) * 0.5f,
            (float)(gridCountZ - 1) * 0.5f);

        float3 gridPosition = float3((float)x, (float)y, (float)z) - halfExtent;
        particle.translate = gInteraction.gridCenter + gridPosition * spacing;
        particle.isAlive = 1;
    }

    gParticles[index] = particle;
}
