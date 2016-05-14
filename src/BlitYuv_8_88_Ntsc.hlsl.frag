WATERMARK_PREAMBLE_HLSL

R"*###*(

static const float LumaMin = 16.0/255.0;
static const float LumaMax = 253.0/255.0;
static const float ChromaVRed = 1.5958;
static const float ChromaUGreen = -0.39173;
static const float ChromaVGreen = -0.81290;
static const float ChromaUBlue = 2.017;


//	ChromaUV_88
float2 GetChromaUv(SamplerState TextureSampler,Texture2D Texture,float2 uv)
{
	float2 ChromaUv = Texture.Sample( TextureSampler, uv ).xy;
	return ChromaUv;
}

)*###*"

#include "BlitYuv.hlsl.frag"
