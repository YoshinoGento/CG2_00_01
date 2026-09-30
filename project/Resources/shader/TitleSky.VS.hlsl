#include "TitleSky.hlsli"
SkyVertex main(float4 position : POSITION) {
    SkyVertex result;
    result.position = mul(position, wvp);
    result.position.z = result.position.w;
    result.direction = position.xyz;
    return result;
}
