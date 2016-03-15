WATERMARK_PREAMBLE_GLSL

R"*###*(

varying highp vec2 oTexCoord;
uniform sampler2D Texture0;
//const highp mat3 Transform = mat3(	1,0,0,	0,1,0,	0,0,1	);
uniform highp mat3 Transform;
void main()
{
	highp vec2 uv = (vec3(oTexCoord.x,oTexCoord.y,1)*Transform).xy;
	highp vec4 rgba = vec4( mix(0.6,1.0,max(uv.x,uv.y)),uv.x*0.3,uv.y*0.3,1);

)*###*"

#include "BlitWatermark.glsl.frag"

R"*###*(
	
	gl_FragColor = rgba;
}

)*###*"



