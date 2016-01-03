R"*###*(


varying highp vec2 oTexCoord;
uniform sampler2D Texture0;	//	rgba
uniform sampler2D Texture1;	//	palette
uniform vec2 Texture1_PixelSize;
const highp mat3 Transform = mat3(	1,0,0,	0,1,0,	0,0,1	);
const int TransparentPixelIndex = 0;

float GetColourScore(vec4 a,vec4 b)
{
	vec3 Diff = abs(b.xyz - a.xyz);
	float Score = 1.0 - ( ( Diff.x + Diff.y + Diff.z) / 3.0f );
	return Score;
}

void main()
{
	highp vec2 uv = (vec3(oTexCoord.x,oTexCoord.y,1)*Transform).xy;
	highp vec4 rgba = texture2D(Texture0,oTexCoord);
	float NearestScore = -1;
	float NearestIndex = 1;
	
	for ( float i=0;	i<Texture1_PixelSize.x;	i++ )
	{
		highp vec4 PalColour = texture2D( Texture1, vec2( i / Texture1_PixelSize.x, 0 ) );
		float Score = GetColourScore( rgba, PalColour );
		
		//	skip transparent
		bool BetterScore = ( Score > NearestScore ) && (i!=TransparentPixelIndex);
		NearestScore = BetterScore ? Score : NearestScore;
		NearestIndex = BetterScore ? i : NearestIndex;
	}
	
	//	if input is transparent, write transparent
	if ( rgba.w == 0 )
		NearestIndex = TransparentPixelIndex;

	float NearestIndexf = NearestIndex / Texture1_PixelSize.x;
	rgba = vec4( NearestIndexf, 0, 0, 1 );	//	only x matters
	gl_FragColor = rgba;
}




)*###*"
