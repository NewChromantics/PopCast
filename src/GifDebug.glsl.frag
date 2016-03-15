R"*###*(



varying highp vec2 oTexCoord;
uniform sampler2D Texture0;	//	rgba
uniform sampler2D Texture1;	//	palette
uniform vec2 Texture1_PixelSize;

void main()
{
	gl_FragColor = vec4( oTexCoord.y, 0, 0, 1 );	//	only x matters
}




)*###*"