WATERMARK_PREAMBLE_HLSL

R"*###*(

static const float LumaMin = 16.0/255.0;
static const float LumaMax = 253.0/255.0;
static const float ChromaVRed = 1.5958;
static const float ChromaUGreen = -0.39173;
static const float ChromaVGreen = -0.81290;
static const float ChromaUBlue = 2.017;


//	ChromaUV_8_8
float2 GetChromaUv(SamplerState TextureSampler,Texture2D Texture,float2 uv)
{
	float ChromaU = Texture.Sample(Texture,uv*float2(1.0,0.5)+float2(0,0)).x;
	float ChromaV = Texture.Sample(Texture,uv*float2(1.0,0.5)+float2(0,0.5)).x;
	float2 ChromaUv = float2(ChromaU,ChromaV);
	return ChromaUv;
}

)*###*"

#include "BlitYuv.hlsl.frag"

