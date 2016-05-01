WATERMARK_PREAMBLE_GLSL

R"*###*(

varying highp vec2 oTexCoord;
uniform sampler2D Texture0;
//"const highp mat3 Transform = mat3(	1,0,0,	0,1,0,	0,0,1	);\n"
uniform highp mat3 Transform;
void main()
{
	highp vec2 uv = (vec3(oTexCoord.x,oTexCoord.y,1)*Transform).xy;
	highp vec4 rgba = texture2D(Texture0,uv);

)*###*"

#include "BlitWatermark.glsl.frag"

//	on GLES2 on ios, alpha comes out zero. I'm SURE this breaks some platforms (bad compilers?) android maybe. so very specific atm
#if defined(TARGET_IOS)
"	rgba.a = 1.0;\n"
#endif

R"*###*(
	
	gl_FragColor = rgba;
}

)*###*"
