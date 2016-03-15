WATERMARK_PREAMBLE_GLSL

R"*###*(

varying highp vec2 oTexCoord;
uniform highp mat3 Transform;
uniform sampler2D Texture0;
const float DepthInvalid = 0.f/1000.f;
const float DepthMax = 10000.f;
uniform highp mat3 Transform;


void main()
{
	highp vec2 uv = (vec3(oTexCoord.x,oTexCoord.y,1)*Transform).xy;
	highp vec4 rgba = texture2D(Texture0,uv);
	//	get depth as a float
	highp float Depth = ( (rgba.x * 255.f) + (rgba.y * 255.f * 255.f) ) / DepthMax;
	//	identify bad depth values and write them to the blue channel
	rgba.z = ( Depth <= DepthInvalid ) ? 1 : 0;
	//	convert depth back to a full range float
	Depth = Depth * ((255.f*255.f)+255.f);
	rgba.x = mod( Depth, 255.f );
	rgba.y = (Depth-rgba.x) / (255.f*255.f);
	
)*###*"
	
#include "BlitWatermark.glsl.frag"
	
R"*###*(
	
	gl_FragColor = rgba;
}

)*###*"



