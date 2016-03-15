
R"*###*(

varying highp vec2 oTexCoord;
uniform sampler2D Texture0;
uniform sampler2D Texture1;
uniform highp mat3 Transform;

void main()
{
	highp vec2 uv = (vec3(oTexCoord.x,oTexCoord.y,1)*Transform).xy;
	//	scale video luma to to 0..1
	float Luma = texture2D(Texture0,uv).x;
	Luma = (Luma-LumaMin) / (LumaMax-LumaMin);
	
	//	get chroma value from specialisation
	vec2 ChromaUv = GetChromaUv( Texture1, uv );
	
	//	convert chroma from 0..1 to -0.5..0.5
	ChromaUv -= vec2(0.5,0.5);
	//	convert to rgb
	vec4 rgba;
	rgba.r = Luma + ChromaVRed * ChromaUv.y;
	rgba.g = Luma + ChromaUGreen * ChromaUv.x + ChromaVGreen * ChromaUv.y;
	rgba.b = Luma + ChromaUBlue * ChromaUv.x;
	rgba.a = 1.0;

)*###*"

#include "BlitWatermark.glsl.frag"

R"*###*(
	
	gl_FragColor = rgba;
}

)*###*"
