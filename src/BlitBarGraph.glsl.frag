R"*###*(

varying highp vec2 oTexCoord;
uniform highp vec3 Colour;
uniform highp vec4 BarRect;
void main()
{
	highp vec2 uv = oTexCoord;
	highp vec4 rgba = vec4( Colour.x, Colour.y, Colour.z, 1 );
	if ( uv.x < BarRect.x || uv.y < BarRect.y || uv.x > BarRect.x+BarRect.z || uv.y > BarRect.y+BarRect.w )
		discard;
	gl_FragColor = rgba;
}

)*###*"

