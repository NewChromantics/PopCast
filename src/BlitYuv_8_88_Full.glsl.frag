WATERMARK_PREAMBLE_GLSL

R"*###*(

const float LumaMin = 0.0;
const float LumaMax = 1.0;
const float ChromaVRed = 1.4;
const float ChromaUGreen = -0.343;
const float ChromaVGreen = -0.711;
const float ChromaUBlue = 1.765;

//	ChromaUV_88
vec2 GetChromaUv(sampler2D Texture,vec2 uv)
{
	vec2 ChromaUv = texture2D(Texture,uv).xy;
	return ChromaUv;
}

)*###*"

#include "BlitYuv.glsl.frag"
