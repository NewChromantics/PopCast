WATERMARK_PREAMBLE_HLSL

R"*###*(

static const float LumaMin = 0.0;
static const float LumaMax = 1.0;
static const float ChromaVRed = 1.4;
static const float ChromaUGreen = -0.343;
static const float ChromaVGreen = -0.711;
static const float ChromaUBlue = 1.765;


//	ChromaUV_88
float2 GetChromaUv(SamplerState TextureSampler,Texture2D Texture,float2 uv)
{
	float2 ChromaUv = Texture.Sample( TextureSampler, uv ).xy;
	return ChromaUv;
}

)*###*"

#include "BlitYuv.hlsl.frag"
