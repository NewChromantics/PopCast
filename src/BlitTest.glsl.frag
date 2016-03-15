WATERMARK_PREAMBLE_GLSL

R"*###*(

varying highp vec2 oTexCoord;
uniform highp mat3 Transform;
void main()
{
	highp vec2 uv = (vec3(oTexCoord.x,oTexCoord.y,1)*Transform).xy;
	highp vec4 rgba = vec4(uv.x,uv.y,0,1);
	if ( uv.x < 0.0 )	rgba.xyz=vec3(0,0,1);
	if ( uv.y < 0.0 )	rgba.xyz=vec3(1,1,0);


)*###*"
	
#include "BlitWatermark.glsl.frag"

R"*###*(
	
	gl_FragColor = rgba;
}

)*###*"

