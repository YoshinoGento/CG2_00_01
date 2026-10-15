#include "Sprite2D.hlsli"
Texture2D<float4> sceneImage : register(t0);
Texture2D<float4> reflectionImage : register(t1);
Texture2D<float4> titleLogo : register(t2);
SamplerState imageSampler : register(s0);
cbuffer PondConstants : register(b0) {
    float4 pondX, pondZ, pondDivisor;
    float4 surface; // fallback mirror line, seconds, night, planar ready
    float4 waterColor;
    float4 geometry; // center XZ, radii XZ
    float4 inflow; // normalized mouth XZ, camera height above water, reserved
    float4 sunlight; // world direction, exposure
    float4 camera; // world X, height above water, world Z, reserved
};
float2 BoundedUV(float2 uv) { return clamp(uv,float2(.001,.002),float2(.999,.998)); }
float4 ReadScene(float2 uv) { return sceneImage.SampleLevel(imageSampler,BoundedUV(uv),0); }
float3 ReadReflection(float2 uv) { return reflectionImage.SampleLevel(imageSampler,BoundedUV(uv),0).rgb; }
float Hash(float2 p) { return frac(sin(dot(p,float2(127.1,311.7)))*43758.5453); }
float4 main(VertexShaderOutput input) : SV_Target0 {
    float3 screen = float3(input.texcoord,1);
    float divisor = dot(pondDivisor.xyz,screen);
    float4 original = ReadScene(input.texcoord);
    if (divisor >= -.00001) return original;
    float2 pond = float2(dot(pondX.xyz,screen),dot(pondZ.xyz,screen))/divisor;
    float radius = length(pond);
    if (radius >= .995) return original;
    float edge = 1-smoothstep(.982,.995,radius);
    float2 world = geometry.xy+pond*geometry.zw;
    float time = surface.y;
    // Multiple gentle wavelengths: reflection motion without a full-screen wobble.
    float a = dot(world,float2(2.1,1.3))+time*1.2566371;
    float b = dot(world,float2(-1.4,3.7))-time*1.8849556;
    float c = dot(world,float2(7.2,4.1))+time*2.5132741;
    float2 slope = float2(2.1,1.3)*cos(a)*.012+float2(-1.4,3.7)*cos(b)*.005
        +float2(7.2,4.1)*cos(c)*.0015;
    float2 fromMouth = (pond-inflow.xy)*geometry.zw;
    float distanceToMouth = length(fromMouth);
    float rings = cos(distanceToMouth*16-time*6.2831853)*exp(-distanceToMouth*1.5);
    slope += fromMouth/max(distanceToMouth,.01)*rings*.033;
    float2 distortion = float2(slope.x,-slope.y)*.10;
    float2 uv = surface.w>.5 ? input.texcoord : float2(input.texcoord.x,2*surface.x-input.texcoord.y);
    uv += distortion;
    float3 reflected = ReadReflection(uv)*.54;
    reflected += (ReadReflection(uv+float2(.0014,0))+ReadReflection(uv-float2(.0014,0)))*.17;
    reflected += (ReadReflection(uv+float2(0,.0015))+ReadReflection(uv-float2(0,.0015)))*.06;
    float3 view = normalize(float3(camera.x-world.x,camera.y,camera.z-world.y));
    float fresnel = .56+.33*pow(1-saturate(view.y),3);
    float shallow = smoothstep(.78,.99,radius);
    float light = clamp(sunlight.w,.08,1);
    float pebble = Hash(floor(world*23));
    float3 bottom = lerp(float3(.20,.35,.32),float3(.48,.47,.32),shallow)*light;
    bottom += (pebble-.5)*.035*shallow*light;
    float3 water = lerp(waterColor.rgb*.38,bottom,shallow*.72);
    float3 color = lerp(water,reflected,fresnel-shallow*.22);
    float3 normal = normalize(float3(-slope.x,1,-slope.y));
    float3 halfVector = normalize(view+normalize(sunlight.xyz));
    float sparkle = pow(saturate(dot(normal,halfVector)),160)*smoothstep(.04,.25,sunlight.y);
    color += float3(1,.91,.68)*sparkle*.22*(1-surface.z);
    float foam = pow(saturate(rings),12)*exp(-distanceToMouth*1.4)*.12;
    float wetEdge = smoothstep(.96,.982,radius)*(1-smoothstep(.982,.994,radius));
    color += foam*light+wetEdge*.035*light;
    float breeze = pow(saturate(sin(a*2.3)+sin(b*1.7)*.35)*.72,14)*.025;
    color += breeze*light;
    // The logo's artistic water echo is separate from the geometrically reflected world.
    const float2 logoOrigin = float2(.035,.77), logoSize = float2(.28,.1834);
    float2 logoUV = (input.texcoord+distortion-logoOrigin)/logoSize;
    if (all(logoUV>0) && all(logoUV<1)) {
        float4 echo = titleLogo.SampleLevel(imageSampler,float2(logoUV.x,1-logoUV.y),0);
        color = lerp(color,echo.rgb,echo.a*.36*sqrt(light));
    }
    return float4(lerp(original.rgb,saturate(color),edge),1);
}
