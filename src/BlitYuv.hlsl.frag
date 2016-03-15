
R"*###*(


Texture2D Texture0;
Texture2D Texture1;
SamplerState Texture0Sampler;
SamplerState Texture1Sampler;
//uniform highp mat3 Transform;

struct PixelInputType
{
	float4 position : SV_POSITION;
	float2 tex : TEXCOORD0;
};

float4 Frag(PixelInputType input) : SV_TARGET
{
	float2 uv = input.tex;
	float Luma = Texture0.Sample( Texture0Sampler, uv ).x;
	Luma = (Luma-LumaMin) / (LumaMax-LumaMin);

	//	get chroma value from specialisation
	float2 ChromaUv = GetChromaUv( Texture1Sampler, Texture1, uv );

	//	convert chroma from 0..1 to -0.5..0.5
	ChromaUv -= float2(0.5,0.5);

	//	convert to rgb
	float4 rgba;
	rgba.r = Luma + ChromaVRed * ChromaUv.y;
	rgba.g = Luma + ChromaUGreen * ChromaUv.x + ChromaVGreen * ChromaUv.y;
	rgba.b = Luma + ChromaUBlue * ChromaUv.x;
	rgba.a = 1.0;

)*###*"

#include "BlitWatermark.hlsl.frag"

R"*###*(

	rgba.w=1;
	return rgba;
}

)*###*"
