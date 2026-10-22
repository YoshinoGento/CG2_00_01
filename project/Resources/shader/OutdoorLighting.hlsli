#ifndef OUTDOOR_LIGHTING_HLSLI
#define OUTDOOR_LIGHTING_HLSLI

float3 EvaluateAmbientLight(float3 normal, float3 lightColor, float intensity, float4 skyTint)
{
    const float ambientResponse = 0.18f;
    float3 legacy = lightColor * (ambientResponse * intensity);
    if (skyTint.w <= 0.0f) return legacy;
    // Hemispherical fill is not GI/AO. The lower hemisphere is weaker and warmer.
    float up = saturate(normal.y * 0.5f + 0.5f);
    float3 hemisphere = lerp(float3(0.42f, 0.38f, 0.30f), float3(1,1,1), up);
    float3 outdoor = skyTint.rgb * hemisphere * (ambientResponse * intensity);
    return lerp(legacy, outdoor, saturate(skyTint.w));
}
#endif
