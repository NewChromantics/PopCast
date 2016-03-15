WATERMARK_PREAMBLE_HLSL

R"*###*(

static const float LumaMin = 0.0;
static const float LumaMax = 1.0;
static const float ChromaVRed = 1.4;
static const float ChromaUGreen = -0.343;
static const float ChromaVGreen = -0.711;
static const float ChromaUBlue = 1.765;


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
