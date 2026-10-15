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
    float imageScale;
    float fadeOpacity;
    float shortFade;
    float reserved;
    float4 pondFlight; // source UV, pond enabled, reserved
    float4 pondX, pondZ, pondDivisor;
};
static const int kRingCount = 3;
static const float kRingSpacing = 38;
static const float kPacketWidth = 18;
static const float kWaveFrequency = .16;
static const float kFollowingWaveAttenuation = .62;
static const float kMaximumDisplacement = 14;
static const float kWipeMargin = 32;
static const float kWipeEdge = 3;
static const float kPondWipeEdge = 16;
static const float kPondCoverOpacityStart = .20;
static const float kPondCoverOpacityEnd = .58;
float WipeRadius() {
    float maxRadius = length(max(center,1-center)*resolution)+kWipeMargin;
    // Reveal returns from the perimeter; its passed (outer) side exposes the next scene.
    return revealing > .5 ? lerp(maxRadius,-kWipeMargin,progress) : lerp(-kWipeMargin,maxRadius,progress);
}
float WaveSlope(float distanceFromFront) {
    float envelope = exp(-distanceFromFront*distanceFromFront/(kPacketWidth*kPacketWidth));
    float phase = distanceFromFront*kWaveFrequency;
    return envelope*(cos(phase)-2*distanceFromFront/(kPacketWidth*kPacketWidth*kWaveFrequency)*sin(phase));
}
float3 ReadScene(float2 uv) {
    float2 halfTexel = .5/resolution;
    return sceneImage.SampleLevel(imageSampler,clamp(uv,halfTexel,1-halfTexel),0).rgb;
}
float PondMask(float2 uv) {
    float3 screen = float3(uv,1);
    float divisor = dot(pondDivisor.xyz,screen);
    if (divisor >= -.00001) return 0;
    float2 pond = float2(dot(pondX.xyz,screen),dot(pondZ.xyz,screen))/divisor;
    return 1-smoothstep(.94,.99,length(pond));
}
float4 main(VertexShaderOutput input) : SV_Target0 {
    if (shortFade > .5) {
        const float3 fadeColor = float3(.012,.020,.017);
        const float opacity = saturate(fadeOpacity);
        if (opacity >= 1) return float4(fadeColor,1);
        const float2 uv = .5+(input.texcoord-.5)/max(imageScale,.01);
        // A retreat may expose the snapshot border; do not stretch edge pixels into it.
        const float3 image = any(uv < 0) || any(uv > 1) ? fadeColor : ReadScene(uv);
        return float4(lerp(image,fadeColor,opacity),1);
    }
    const float3 water = float3(.012,.028,.035);
    // These endpoints must be opaque across scene resource replacement.
    if (transitioning > .5 && ((revealing < .5 && progress >= 1) || (revealing > .5 && progress <= 0)))
        return float4(water,1);
    if (transitioning > .5 && revealing > .5 && progress >= 1)
        return float4(ReadScene(input.texcoord),1);
    float2 pixel = input.texcoord*resolution;
    float2 displacement = 0;
    float waveAmount = 0;
    float highlight = 0;
    float fallingDrop = 0;
    float poolBlend = 1;
    float ellipseScale = 1;
    if (pondFlight.z > .5 && revealing < .5) {
        poolBlend = smoothstep(.08,.42,progress);
        ellipseScale = lerp(.32,1,poolBlend);
    }
    [unroll] for (int i=0; i<4; ++i) {
        if (drops[i].w < .5) continue;
        float age = drops[i].z;
        float2 origin = drops[i].xy*resolution;
        float waveAge = age-fallSeconds;
        if (waveAge < 0) {
            float flight = saturate(age/fallSeconds);
            float2 dropPixel = pondFlight.z > .5 ?
                lerp(pondFlight.xy*resolution,origin,flight*flight) : origin-float2(0,120*(1-flight*flight));
            float2 ellipse = (pixel-dropPixel)/(pondFlight.z > .5 ? float2(6,11) : float2(3,6));
            float visibility = pondFlight.z > .5 ? .6+.4*sin(flight*3.14159265) : sin(flight*3.14159265);
            fallingDrop += (1-smoothstep(.45,1,length(ellipse)))*visibility;
            continue;
        }
        float2 radial = pixel-origin;
        float2 metric = radial/float2(1,ellipseScale);
        float distanceFromCenter = length(metric);
        float2 direction = metric/max(distanceFromCenter,1)*float2(1,ellipseScale);
        float waveLifetime = max(.01,dropLifetime-fallSeconds);
        float fade = (1-smoothstep(waveLifetime*.63,waveLifetime,waveAge))*exp(-waveAge*.55);
        float localSlope = 0;
        [unroll] for (int ring=0; ring<kRingCount; ++ring) {
            // Pond cover follows the same front as its refractive wave, not a second wipe.
            float radius = (pondFlight.z > .5 ? WipeRadius() : waveAge*wavePixelsPerSecond)-ring*kRingSpacing;
            if (radius <= 0) continue;
            float d = distanceFromCenter-radius;
            // Derivative of a damped radial wave packet drives refraction, not a painted circle.
            float slope = WaveSlope(d);
            localSlope += slope*pow(kFollowingWaveAttenuation,ring)*smoothstep(0,20,radius)*fade;
        }
        displacement += direction*localSlope*refractionPixels;
        waveAmount += abs(localSlope);
        highlight += localSlope*.025;
    }
    if (pondFlight.z > .5 && revealing < .5) {
        float waterOnly = lerp(PondMask(input.texcoord),1,poolBlend);
        displacement *= waterOnly; waveAmount *= waterOnly; highlight *= waterOnly;
    }
    if (transitioning > .5 && revealing > .5) {
        float2 radial = pixel-center*resolution;
        float distanceFromCenter = length(radial);
        float localSlope = 0;
        float fade = smoothstep(0,.05,progress)*(1-smoothstep(.85,1,progress));
        [unroll] for (int ring=0; ring<kRingCount; ++ring) {
            // Following reflected packets trail on the already revealed outer side.
            float radius = WipeRadius()+ring*kRingSpacing;
            localSlope -= WaveSlope(distanceFromCenter-radius)*pow(kFollowingWaveAttenuation,ring)*fade;
        }
        displacement += radial/max(distanceFromCenter,1)*localSlope*refractionPixels;
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
        float d = length((pixel-center*resolution)/float2(1,ellipseScale))-WipeRadius();
        bool pondCover = pondFlight.z > .5 && revealing < .5;
        float edgeWidth = pondCover ? kPondWipeEdge : kWipeEdge;
        float inside = 1-smoothstep(-edgeWidth,edgeWidth,d);
        if (pondCover) {
            inside *= lerp(PondMask(input.texcoord),1,poolBlend);
            // Let the reflected image carry the first three fronts before the opaque cover develops.
            inside *= smoothstep(kPondCoverOpacityStart,kPondCoverOpacityEnd,progress);
        }
        color = lerp(color,water,inside);
    }
    return float4(color,1);
}
