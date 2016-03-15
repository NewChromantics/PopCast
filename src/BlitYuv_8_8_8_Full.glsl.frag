WATERMARK_PREAMBLE_GLSL

R"*###*(

const float LumaMin = 0.0;
const float LumaMax = 1.0;
const float ChromaVRed = 1.4;
const float ChromaUGreen = -0.343;
const float ChromaVGreen = -0.711;
const float ChromaUBlue = 1.765;


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
