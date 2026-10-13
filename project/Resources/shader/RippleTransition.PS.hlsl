#include "Sprite2D.hlsli"
Texture2D<float4> sceneImage : register(t0);
SamplerState imageSampler : register(s0);
cbuffer TransitionConstants : register(b0) {
    float2 center;
    float progress;
    float revealing;
    float2 resolution;
    float fallSeconds;
    float transitioning;
    float4 drops[4]; // normalized center, age in seconds, active
    float dropLifetime;
    float wavePixelsPerSecond;
    float refractionPixels;
    float blurPixels;
};
static const int kRingCount = 3;
static const float kRingSpacing = 38;
static const float kPacketWidth = 18;
static const float kWaveFrequency = .16;
static const float kFollowingWaveAttenuation = .62;
static const float kMaximumDisplacement = 14;
float3 ReadScene(float2 uv) {
    float2 halfTexel = .5/resolution;
    return sceneImage.SampleLevel(imageSampler,clamp(uv,halfTexel,1-halfTexel),0).rgb;
}
float4 main(VertexShaderOutput input) : SV_Target0 {
    const float3 water = float3(.012,.028,.035);
    // These endpoints must be opaque across scene resource replacement.
    if (transitioning > .5 && ((revealing < .5 && progress >= 1) || (revealing > .5 && progress <= 0)))
        return float4(water,1);
    float2 pixel = input.texcoord*resolution;
    float2 displacement = 0;
    float waveAmount = 0;
    float highlight = 0;
    float fallingDrop = 0;
    [unroll] for (int i=0; i<4; ++i) {
        if (drops[i].w < .5) continue;
        float age = drops[i].z;
        float2 origin = drops[i].xy*resolution;
        float waveAge = age-fallSeconds;
        if (waveAge < 0) {
            float flight = saturate(age/fallSeconds);
            float2 dropPixel = origin-float2(0,120*(1-flight*flight));
            float2 ellipse = (pixel-dropPixel)/float2(3,6);
            fallingDrop += (1-smoothstep(.45,1,length(ellipse)))*sin(flight*3.14159265);
            continue;
        }
        float2 radial = pixel-origin;
        float distanceFromCenter = length(radial);
        float2 direction = radial/max(distanceFromCenter,1);
        float waveLifetime = max(.01,dropLifetime-fallSeconds);
        float fade = (1-smoothstep(waveLifetime*.63,waveLifetime,waveAge))*exp(-waveAge*.55);
        float localSlope = 0;
        [unroll] for (int ring=0; ring<kRingCount; ++ring) {
            float radius = waveAge*wavePixelsPerSecond-ring*kRingSpacing;
            if (radius <= 0) continue;
            float d = distanceFromCenter-radius;
            float envelope = exp(-d*d/(kPacketWidth*kPacketWidth));
            float phase = d*kWaveFrequency;
            // Derivative of a damped radial wave packet drives refraction, not a painted circle.
            float slope = envelope*(cos(phase)-2*d/(kPacketWidth*kPacketWidth*kWaveFrequency)*sin(phase));
            localSlope += slope*pow(kFollowingWaveAttenuation,ring)*smoothstep(0,20,radius)*fade;
        }
        displacement += direction*localSlope*refractionPixels;
        waveAmount += abs(localSlope);
        highlight += localSlope*.025;
    }
    // Bound overlapping drops and clamp every sample to valid edge texels.
    displacement = clamp(displacement,-kMaximumDisplacement,kMaximumDisplacement);
    float2 uv = input.texcoord+displacement/resolution;
    float3 color = ReadScene(uv);
    float blur = saturate(waveAmount)*blurPixels;
    if (blur > .01) {
        float2 offset = blur/resolution;
        color = color*.4+(ReadScene(uv+float2(offset.x,0))+ReadScene(uv-float2(offset.x,0))+
            ReadScene(uv+float2(0,offset.y))+ReadScene(uv-float2(0,offset.y)))*.15;
    }
    color = max(0,color+clamp(highlight,-.05,.05));
    color = lerp(color,float3(.65,.82,.85),saturate(fallingDrop)*.75);
    if (transitioning > .5) {
        float maxRadius = length(max(center,1-center)*resolution)+32;
        float radius = lerp(-32,maxRadius,progress);
        float d = length(pixel-center*resolution)-radius;
        float inside = 1-smoothstep(-3,3,d);
        float cover = revealing > .5 ? 1-inside : inside;
        color = lerp(color,water,cover);
    }
    return float4(color,1);
}
