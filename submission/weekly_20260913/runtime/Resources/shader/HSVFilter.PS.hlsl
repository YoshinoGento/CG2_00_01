#include "CopyImage.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct HSVFilterParam
{
    float hue;
    float saturation;
    float value;
    float padding;
};

ConstantBuffer<HSVFilterParam> gHSVFilterParam : register(b0);

float WrapValue(float value, float minRange, float maxRange)
{
    float range = maxRange - minRange;
    float modValue = fmod(value - minRange, range);

    if (modValue < 0.0f)
    {
        modValue += range;
    }

    return minRange + modValue;
}

float3 RGBToHSV(float3 rgb)
{
    float maxValue = max(rgb.r, max(rgb.g, rgb.b));
    float minValue = min(rgb.r, min(rgb.g, rgb.b));
    float delta = maxValue - minValue;

    float hue = 0.0f;
    if (delta > 0.00001f)
    {
        if (maxValue == rgb.r)
        {
            hue = (rgb.g - rgb.b) / delta;
            if (hue < 0.0f)
            {
                hue += 6.0f;
            }
        }
        else if (maxValue == rgb.g)
        {
            hue = ((rgb.b - rgb.r) / delta) + 2.0f;
        }
        else
        {
            hue = ((rgb.r - rgb.g) / delta) + 4.0f;
        }
        hue /= 6.0f;
    }

    float saturation = maxValue > 0.00001f ? delta / maxValue : 0.0f;
    return float3(hue, saturation, maxValue);
}

float3 HSVToRGB(float3 hsv)
{
    float hue = WrapValue(hsv.x, 0.0f, 1.0f) * 6.0f;
    float saturation = saturate(hsv.y);
    float value = saturate(hsv.z);

    float chroma = value * saturation;
    float x = chroma * (1.0f - abs(fmod(hue, 2.0f) - 1.0f));
    float m = value - chroma;

    float3 rgb = float3(0.0f, 0.0f, 0.0f);
    if (hue < 1.0f)
    {
        rgb = float3(chroma, x, 0.0f);
    }
    else if (hue < 2.0f)
    {
        rgb = float3(x, chroma, 0.0f);
    }
    else if (hue < 3.0f)
    {
        rgb = float3(0.0f, chroma, x);
    }
    else if (hue < 4.0f)
    {
        rgb = float3(0.0f, x, chroma);
    }
    else if (hue < 5.0f)
    {
        rgb = float3(x, 0.0f, chroma);
    }
    else
    {
        rgb = float3(chroma, 0.0f, x);
    }

    return rgb + m;
}

float4 main(VertexShaderOutput input) : SV_TARGET0
{
    float4 textureColor = gTexture.Sample(gSampler, input.texcoord);

    float3 hsv = RGBToHSV(textureColor.rgb);
    hsv.x = WrapValue(hsv.x + gHSVFilterParam.hue, 0.0f, 1.0f);
    hsv.y = saturate(hsv.y + gHSVFilterParam.saturation);
    hsv.z = saturate(hsv.z + gHSVFilterParam.value);

    float3 rgb = HSVToRGB(hsv);
    return float4(rgb, textureColor.a);
}
