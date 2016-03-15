WATERMARK_PREAMBLE_GLSL

R"*###*(

varying highp vec2 oTexCoord;
uniform sampler2DRect Texture0;
uniform highp vec2 Texture0_PixelSize;
//"const highp mat3 Transform = mat3(	1,0,0,	0,1,0,	0,0,1	);
uniform highp mat3 Transform;
void main()
{
	highp vec2 uv = (vec3(oTexCoord.x,oTexCoord.y,1)*Transform).xy;
	highp vec4 rgba = texture2DRect(Texture0,uv*Texture0_PixelSize);


)*###*"
	
#include "BlitWatermark.glsl.frag"
	
R"*###*(
	
	gl_FragColor = rgba;
}

)*###*"


