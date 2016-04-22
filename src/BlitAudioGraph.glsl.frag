R"*###*(

uniform sampler2D AudioData;
uniform highp int AudioDataWidth;	//	width of texture (can be wider than sample size)
uniform highp int SampleCount;
uniform highp int TimeIndex;

uniform highp vec3 TimeColour;
uniform highp vec3 AheadColour;
uniform highp vec3 BehindColour;
uniform highp vec3 ErrorColour;

const float Zoom = 4.0;
	
varying highp vec2 oTexCoord;
void main()
{
	highp vec2 uv = oTexCoord;
	
	//	work out sample index
	float Maxu = float(SampleCount) / float(AudioDataWidth);
	float Sampleu = uv.x * Maxu;
	float Timeu = (float(TimeIndex) / float(SampleCount)) * Maxu;
	
	vec3 rgb = uv.x < Timeu ? BehindColour : AheadColour;
	
	/*
	//	Draw time bar
	float TimeBarHalfWidth = 0.01;
	if ( uv.x >= Timeu - TimeBarHalfWidth && uv.x <= Timeu + TimeBarHalfWidth )
	{
		gl_FragColor = vec4( TimeColour, 1 );
		return;
	}
	 */
	
	float Sample = texture2D( AudioData, vec2( Sampleu, 0 ) ).r;
	float SampleRange = (Sample * 2.0) - 1.0 ;
	SampleRange *= Zoom;
	float VRange = uv.y * 2.0 - 1.0;
	
	if ( Sample == 0.0 )
	{
		float ErrorBarHalfHeight = 0.01;
		if ( VRange < ErrorBarHalfHeight )
			discard;
		if ( VRange > ErrorBarHalfHeight )
			discard;
		gl_FragColor = vec4( ErrorColour, 1 );
		return;
	}
	
	
	if ( SampleRange < 0.0 )
	{
		if ( VRange > 0.0 )
			discard;
		if ( VRange < SampleRange )
			discard;
	}
	else
	{
		if ( VRange < 0.0 )
			discard;
		if ( VRange > SampleRange )
			discard;
	}
	
	gl_FragColor = vec4( rgb, 1 );
}

)*###*"

