WATERMARK_PREAMBLE_HLSL

R"*###*(

static const float LumaMin = 16.0/255.0;
static const float LumaMax = 253.0/255.0;
static const float ChromaVRed = 1.5958;
static const float ChromaUGreen = -0.39173;
static const float ChromaVGreen = -0.81290;
static const float ChromaUBlue = 2.017;



Texture2D Texture0;
SamplerState Texture0Sampler;
//uniform highp mat3 Transform;

struct PixelInputType
{
	float4 position : SV_POSITION;
	float2 tex : TEXCOORD0;
};

float4 Frag(PixelInputType input) : SV_TARGET
{
	//	todo: transform
	float2 uv = input.tex;

	//	for DXGI_FORMAT_R8G8_B8G8_UNORM 
	float4 YuYv = Texture0.Sample( Texture0Sampler, uv );
	//	gr: do subsampling properly! only using half the lumas!
	float Luma = YuYv.x;
	//float2 ChromaUv = YuYv.yw;
	float2 ChromaUv = 0.5f;

	/*
	//	for Yuv 844
	//	expecting input texture of RG
	float2 Luma8Chroma44 = Texture0.Sample( Texture0Sampler, uv ).xy;
	float Luma = Luma8Chroma44.x;

	//0=8-bit Y'0; Byte 1=8-bit Cb; Byte 2=8-bit Y'1; Byte 3=8-bit Cr.
	//	split 4 & 4 bits to 0..1,0..1
	float Chroma44 = Luma8Chroma44.y * 255.f;
	//	/ 4 bits (16) for major
	//	frac(mod) for minor
	//float2 ChromaUv = float2( floor(Chroma44 / 16.f) / 16.f, frac(Chroma44 / 16.f) );
	//float2 ChromaUv = float2( floor(Chroma44 / 16.f) / 16.f, 0 );
	float2 ChromaUv = float2( 0, frac(Chroma44 / 16.f) );
	*/

	Luma = (Luma-LumaMin) / (LumaMax-LumaMin);

	//	convert chroma from 0..1 to -0.5..0.5
	ChromaUv -= float2(0.5,0.5);

	//	test luma
	//	gr: would be nice to propogate the mMergeYuv option
	//float4 rgba = float4( Luma, Luma, Luma, 1.0 );
		
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
