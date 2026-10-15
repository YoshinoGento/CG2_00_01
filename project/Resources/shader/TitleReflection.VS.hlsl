#include "TitleReflection.hlsli"
ReflectionVertex main(float4 position : POSITION, float3 normal : NORMAL, float2 uv : TEXCOORD) {
    ReflectionVertex output;
    float4 original = mul(position,world);
    output.aboveWater = original.y-settings.x;
    original.y = 2*settings.x-original.y;
    output.position = mul(original,viewProjection);
    output.uv = uv;
    // Cofactors provide inverse-transpose normals for nonuniform scale/shear.
    float3x3 cofactor = float3x3(cross(world[1].xyz,world[2].xyz),
        cross(world[2].xyz,world[0].xyz),cross(world[0].xyz,world[1].xyz));
    output.normal = normalize(mul(normal,cofactor));
    return output;
}
