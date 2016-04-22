WATERMARK_PREAMBLE_GLSL

R"*###*(

const float LumaMin = 16.0/255.0;
const float LumaMax = 253.0/255.0;
const float ChromaVRed = 1.5958;
const float ChromaUGreen = -0.39173;
const float ChromaVGreen = -0.81290;
const float ChromaUBlue = 2.017;


//	ChromaUV_8_8
vec2 GetChromaUv(sampler2D Texture,vec2 uv)
{
	float ChromaU = texture2D(Texture,uv*vec2(1.0,0.5)+vec2(0,0)).x;
	float ChromaV = texture2D(Texture,uv*vec2(1.0,0.5)+vec2(0,0.5)).x;
	vec2 ChromaUv = vec2(ChromaU,ChromaV);
	return ChromaUv;
}

)*###*"

#include "BlitYuv.glsl.frag"
