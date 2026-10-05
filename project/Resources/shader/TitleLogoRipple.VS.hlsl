#include "Sprite2D.hlsli"
struct Input { float2 position : POSITION0; float2 uv : TEXCOORD0; };
VertexShaderOutput main(Input input) {
    VertexShaderOutput output;
    output.position = float4(input.position,0,1);
    output.texcoord = input.uv;
    return output;
}
