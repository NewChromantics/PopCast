WATERMARK_PREAMBLE_GLSL

R"*###*(

#extension GL_OES_EGL_image_external : require
varying highp  vec2 oTexCoord;
uniform samplerExternalOES Texture0;
//const highp mat3 Transform = mat3(	1,0,0,	0,1,0,	0,0,1	);
uniform highp  mat3 Transform;
void main()
{
	highp vec2 uv = (vec3(oTexCoord.x,oTexCoord.y,1)*Transform).xy;
	highp vec4 rgba = texture2D(Texture0,uv);


)*###*"
	
#include "BlitWatermark.glsl.frag"

R"*###*(
	
	//	on jon's sony android, setting alpha turns the scene greeny/purple!? either here or in the frag. NO idea why. direct copy is fine
	//	rgba.a = 1.0;
	gl_FragColor = rgba;
}

)*###*"

