WATERMARK_PREAMBLE_GLSL

R"*###*(

const float LumaMin = 16.0/255.0;
const float LumaMax = 253.0/255.0;
const float ChromaVRed = 1.5958;
const float ChromaUGreen = -0.39173;
const float ChromaVGreen = -0.81290;
const float ChromaUBlue = 2.017;

//	ChromaUV_88
vec2 GetChromaUv(sampler2D Texture,vec2 uv)
{
	vec2 ChromaUv = texture2D(Texture,uv).xy;
	return ChromaUv;
}

)*###*"

#include "BlitYuv.glsl.frag"
