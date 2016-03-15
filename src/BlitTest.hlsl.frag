WATERMARK_PREAMBLE_HLSL

R"*###*(

struct PixelInputType
{
    float4 position : SV_POSITION;
    float2 tex : TEXCOORD0;
};

float4 Frag(PixelInputType input) : SV_TARGET
{
	float4 rgba = float4(input.tex.x,input.tex.y,0,1);
	float2 uv = input.tex;

)*###*"

#include "BlitWatermark.hlsl.frag"

R"*###*(

    return rgba;
}


)*###*"

