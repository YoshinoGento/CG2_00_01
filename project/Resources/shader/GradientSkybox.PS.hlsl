#include "Skybox.hlsli"

float4 main(VertexShaderOutput input) : SV_TARGET
{
    const float3 direction = normalize(input.texcoord);
    // A deliberately broad transition avoids a visible horizon band or cube seam.
    const float blend = smoothstep(-0.75f, 0.75f, direction.x);
    return lerp(gMaterial.color, gMaterial.secondaryColor, blend);
}
