#include "Sprite2D.hlsli"
cbuffer RippleConstants : register(b0) {
    float4 ripples[2]; // Center pixels, outward radius, lifetime opacity.
    float4 style; // Width, vertical scale, trailing distance, trailing strength.
    float4 tint;
};
Texture2D<float4> glyphMask : register(t0);
SamplerState pointSampler : register(s0);
float4 main(VertexShaderOutput input) : SV_Target0 {
    float mask = glyphMask.Sample(pointSampler,input.texcoord).a;
    if (mask == 0) discard;
    uint width,height;
    glyphMask.GetDimensions(width,height);
    float2 pixel = floor(input.texcoord*float2(width,height)/2)*2+1;
    float alpha = 0;
    [unroll] for (uint i=0; i<2; ++i) {
        float2 offset = pixel-ripples[i].xy;
        offset.y *= style.y;
        float distance = length(offset)-ripples[i].z;
        float primary = distance/style.x;
        float echo = (distance+style.z)/style.x;
        float wave = saturate(exp(-.5*primary*primary)+style.w*exp(-.5*echo*echo));
        // Maximum opacity preserves readable lettering when two impacts overlap.
        alpha = max(alpha,wave*ripples[i].w);
    }
    return float4(tint.rgb,mask*alpha);
}
