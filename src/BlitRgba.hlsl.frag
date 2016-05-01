WATERMARK_PREAMBLE_HLSL

R"*###*(


Texture2D Texture0;
SamplerState Texture0Sampler;
struct PixelInputType
{
	float4 position : SV_POSITION;
	float2 tex : TEXCOORD0;
};

float4 Frag(PixelInputType input) : SV_TARGET
{
	float4 rgba = Texture0.Sample( Texture0Sampler, input.tex );
	float2 uv = input.tex;

)*###*"

#include "BlitWatermark.hlsl.frag"

R"*###*(

	rgba.w=1;
	return rgba;
}


)*###*"

