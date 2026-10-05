#include "TitleSky.hlsli"
Texture2D<float4> cloudTexture : register(t0);
SamplerState skySampler : register(s0);

float Hash(float2 cell) { return frac(sin(dot(cell,float2(127.1,311.7)))*43758.5453); }
float MoonPatch(float2 p, float2 center, float2 radius) {
    return 1-smoothstep(.70,1.05,length((p-center)/radius));
}
float3 MoonSurface(float3 direction, float3 lunar) {
    float3 reference = abs(lunar.y) > .95 ? float3(0,0,1) : float3(0,1,0);
    float3 right = normalize(cross(reference,lunar));
    float3 up = cross(lunar,right);
    float radius = sqrt(max(1-discs.z*discs.z,.000001));
    float2 p = float2(dot(direction,right),dot(direction,up))/radius;
    // Fixed lunar-local texels: surface details do not shimmer as the disc moves.
    p = (floor(p*32)+.5)/32;
    float2 land = p+float2(.06*sin(p.y*14)+.03*cos(p.x*23),.05*sin(p.x*17));
    float maria = max(MoonPatch(land,float2(-.28,.26),float2(.44,.36)),
        MoonPatch(land,float2(.26,.40),float2(.28,.35)));
    maria = max(maria,MoonPatch(land,float2(-.12,-.05),float2(.29,.30)));
    maria = max(maria,MoonPatch(land,float2(-.50,-.26),float2(.22,.26)));
    float crater = MoonPatch(p,float2(.36,-.45),float2(.13,.13));
    float rim = MoonPatch(p,float2(.36,-.45),float2(.18,.18))-crater;
    float3 surface = lerp(float3(.83,.85,.86),float3(.49,.53,.58),maria*.65);
    return surface-crater*.11+rim*.05+(Hash(floor(p*32))-.5)*.045;
}
float OutputDither(uint2 pixel) {
    uint2 low = pixel & 1u;
    uint2 high = (pixel >> 1u) & 1u;
    uint rank = 4u * (2u * (low.x ^ low.y) + low.y)
        + 2u * (high.x ^ high.y) + high.y;
    // Fixed screen-space pattern: one UNORM step peak-to-peak, no temporal flicker.
    return ((float(rank) + .5) / 16.0 - .5) / 255.0;
}
float4 main(SkyVertex input) : SV_TARGET {
    const float pi = 3.14159265359;
    float3 direction = normalize(input.direction);
    float2 uv = float2(frac(atan2(direction.z,direction.x)/(2*pi)+.5+motion.x),
        saturate(acos(clamp(direction.y,-1,1))/pi+animation.x));
    // Stable angular texels preserve cloud silhouettes instead of pixelating the UI/world.
    uv = (floor(uv*motion.zw)+.5)/motion.zw;
    float3 photo = cloudTexture.SampleLevel(skySampler,uv,0).rgb;
    float cloud = smoothstep(.08,.34,min(photo.r,photo.g)-photo.b*.45);
    float altitude = saturate(direction.y*1.65);
    float3 color = lerp(horizonColor.rgb,topColor.rgb,sqrt(altitude));
    float cloudOpacity = cloud*smoothstep(-.04,.16,direction.y);
    float3 solar = normalize(sun.xyz);
    float sunDisc = smoothstep(discs.x,discs.y,dot(direction,solar));
    float3 lunar = normalize(moon.xyz);
    float moonDisc = smoothstep(discs.z,discs.w,dot(direction,lunar));
    color = lerp(color,float3(1.0,.82,.36),sunDisc);
    if (moonDisc > 0) color = lerp(color,MoonSurface(direction,lunar),moonDisc);
    float2 starUv = float2(frac(atan2(direction.z,direction.x)/(2*pi)+.5+animation.y/(2*pi)),
        acos(clamp(direction.y,-1,1))/pi);
    float2 cell = floor(starUv*float2(320,160));
    float random = Hash(cell);
    float starPoint = 1-smoothstep(.08,.22,length(frac(starUv*float2(320,160))-.5));
    float star = starPoint*step(.996,random)*step(.2, direction.y)*animation.z*(1-moonDisc)*(1-sunDisc);
    color += star * (.42+.18*sin(random*20+motion.y*6.2831853));
    // Clouds are foreground: opaque cores hide bodies, thin edges transmit them.
    color = lerp(color,cloudColor.rgb,cloudOpacity);
    // Preserve continuous sky gradients; angular cloud texels carry the pixel-art style.
    return float4(saturate(color + OutputDither(uint2(input.position.xy))),1);
}
