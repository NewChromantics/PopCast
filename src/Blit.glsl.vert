R"*###*(


//"layout(location = 0) in vec3 TexCoord;\n"
attribute vec3 TexCoord;
varying highp vec2 oTexCoord;
void main()
{
   gl_Position = vec4(TexCoord.x,TexCoord.y,0,1);
	//	move to view space 0..1 to -1..1
	gl_Position.xy *= vec2(2,2);
	gl_Position.xy -= vec2(1,1);
	//	flip uv in vertex shader to apply to all
	oTexCoord = vec2( TexCoord.x, 1.0-TexCoord.y);
}




)*###*"
