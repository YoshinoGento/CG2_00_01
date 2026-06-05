struct Particle
{
    float3 translate;
    float3 scale;
    float lifeTime;
    float3 velocity;
    float currentTime;
    float4 color;
};

RWStructuredBuffer<Particle> gParticles : register(u0);

[numthreads(1024, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint index = dispatchThreadId.x;

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

    gParticles[index] = particle;
}
