#if (POPMOVIE_WATERMARK!=0)

R"*###*(

	//	grahams fancy chevron watermark; https://www.shadertoy.com/view/4lSXRW
	float stripexval = mod((1.0-uv.y+0.01)*4.0,0.7);
	float stripeyval = mod((uv.x+0.01)*10.0,1.2);
	bool stripexa = stripexval  >= mix( 0.0, 0.5, stripeyval+0.0 ) && stripexval <= 1.0-mix( 0.0, 0.5, stripeyval+0.0 ) && stripeyval > 0.7;
	bool stripexb = stripexval  >= mix( 0.0, 0.5, stripeyval+0.2 ) && stripexval <= 1.0-mix( 0.0, 0.5, stripeyval+0.2 );
	bool stripexc = stripexval >= mix( 0.0, 0.5, stripeyval+0.4 ) && stripexval <= 1.0-mix( 0.0, 0.5, stripeyval+0.4 );
	bool stripexd = stripexval >= mix( 0.0, 0.5, stripeyval+0.6 ) && stripexval <= 1.0-mix( 0.0, 0.5, stripeyval+0.6 );
	bool stripex = ( stripexa && !stripexb ) || ( stripexc && !stripexd );
	bool stripey = stripeyval > 0.30;
	if ( stripex && stripey  )
	{
		rgba.rgb = WATERMARK_RGB;
	}

)*###*"

#endif

""

