#include "TitleGrass.hlsli"
struct GrassInput {
    float4 position : POSITION0;
    float3 normal : NORMAL0;
    float2 texcoord : TEXCOORD0;
    float4 positionScale : TEXCOORD1;
    float4 tint : COLOR0;
    float2 yaw : TEXCOORD2;
};
float LandscapeHeight(float2 location) {
    float distance = max(max(abs(location.x)-18,location.y-32),0);
    float t = saturate(distance/18);
    return -.235+t*t*(3-2*t)*(.45+.16*sin(location.x*.12)+.12*cos(location.y*.11));
}
float3 TerrainHeightGradient(float2 location) {
    // Match generate_title_landscape.py's five-unit triangulated ground exactly.
    float2 origin = float2(-50,-74)+floor((location-float2(-50,-74))/5)*5;
    float2 fraction = (location-origin)/5;
    float a = LandscapeHeight(origin), b = LandscapeHeight(origin+float2(0,5));
    float c = LandscapeHeight(origin+float2(5,5)), d = LandscapeHeight(origin+float2(5,0));
    float2 gradient = fraction.y >= fraction.x ? float2(c-b,b-a)/5 : float2(d-a,c-d)/5;
    return float3(a+dot(gradient,location-origin),gradient);
}
GrassOutput main(GrassInput input) {
    GrassOutput output;
    float c = input.yaw.x, s = input.yaw.y;
    float3 local = input.position.xyz*input.positionScale.w;
    float3 world = float3(local.x*c+local.z*s,local.y,-local.x*s+local.z*c)+input.positionScale.xyz;
    float3 normal = float3(input.normal.x*c+input.normal.z*s,input.normal.y,-input.normal.x*s+input.normal.z*c);
    float3 terrain = TerrainHeightGradient(world.xz);
    world.y += terrain.x-input.positionScale.y;
    normal.xz -= terrain.yz*normal.y;
    // Camera-right is the fixed world axis of cloud motion; signed phase reverses with the wind.
    float phase = wind.z-dot(world.xz,wave.xy)*wave.z+input.texcoord.y*wave.w;
    float crest = .5+.5*sin(phase);
    float gust = .08+.92*crest*crest;
    float height = max(local.y,0);
    // Quadratic height weight leaves roots and their tangent fixed on the terrain.
    float weight = height*height*wind.w*gust;
    world.xz += wind.xy*weight;
    float gustDerivative = .92*crest*cos(phase);
    float2 horizontalGradient = -wave.xy*wave.z*gustDerivative*height*height*wind.w;
    float verticalDerivative = 2*height*wind.w*gust;
    horizontalGradient -= terrain.yz*verticalDerivative;
    float3 gradient = float3(horizontalGradient.x,verticalDerivative,horizontalGradient.y);
    float3 displacement = float3(wind.x,0,wind.y);
    // Inverse-transpose of I + displacement * gradient^T; spatial wave gradients matter too.
    normal -= gradient*dot(displacement,normal)/(1+dot(displacement,gradient));
    output.position = mul(float4(world,1),viewProjection);
    output.worldPosition = world;
    output.normal = normalize(normal);
    output.tint = input.tint;
    output.shade = input.texcoord.x;
    output.wave = crest;
    return output;
}
