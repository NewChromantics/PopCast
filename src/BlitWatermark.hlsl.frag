#if (POPMOVIE_WATERMARK!=0)

R"*###*(

	//	fmod in hlsl isn't exactly the same as GLSL mod()
	//	http://stackoverflow.com/a/7614734
	//	x - y * trunc(x/y). 
		
	#define MODULOUS( a, b )	fmod( a, b )
	#define vec2	float2
	#define vec3	float3
	#define vec4	float4

	#define InsideRectf(p,Minx,Miny,Maxx,Maxy)	(p.x >= Minx && p.x <= Maxx && p.y >= Miny && p.y <= Maxy)
	#define InsideRect(p,Rect)					InsideRectf( p, Rect.x, Rect.y, Rect.z, Rect.w )
	
	//	opengl only
	//#define CORRECT_UV(v)						(1.0-v)
	#define CORRECT_UV(v)						(v)

	//	box uv
    //float AspectRatio = iResolution.x/iResolution.y;
	float AspectRatio = 4.0/3.0;
    float BlocksWide = 8.0;
    //	align to middle for top & bottom
    float onev = MODULOUS( BlocksWide / AspectRatio , 1.0 );
    vec2 boxuv = vec2( MODULOUS( uv.x * BlocksWide, 1.00 ), MODULOUS( (CORRECT_UV(uv.y)-onev) * BlocksWide / AspectRatio , 1.0 ) );
   
    vec3 OrangeColour = vec3( 253.0/255.0, 166.0/255.0, 25.0/255.0 );
    vec3 StickColour = vec3( 191.0/255.0, 147.0/255.0, 79.0/255.0 );
    vec3 StickShadowColour = vec3( 103.0/255.0, 80.0/255.0, 43.0/255.0 );
    vec3 HighlightColour = vec3( 255.0/255.0, 255.0/255.0, 255.0/255.0 );
  
    //	draw orange
    vec4 PopsicleRect = vec4( 0.35, 0.20, 0.65, 0.70 );
    vec4 StickRect = vec4( 0.46, PopsicleRect.w, 0.54, 0.85 );
    vec4 StickShadowRect = StickRect;
    StickShadowRect.w = StickRect.y + (StickRect.w-StickRect.y)*0.20;

    if ( boxuv.x >= PopsicleRect.x && boxuv.x <= PopsicleRect.z &&
    	boxuv.y >= PopsicleRect.y && boxuv.y <= PopsicleRect.w )
    {
        //	calc curve
        float CurveSize = 0.100;
        float HighlightCurveMin = 0.40;
        float HighlightCurveMax = 0.7;
        bool InCurve = false;
        bool InHighlight = false;
        vec2 TopLeftCurvePivot = vec2(PopsicleRect.x+CurveSize,PopsicleRect.y+CurveSize);
        vec2 TopLeftDelta = boxuv - TopLeftCurvePivot;
        if ( InsideRectf( boxuv, 0.0, 0.0, PopsicleRect.x+CurveSize, PopsicleRect.y+CurveSize ) )
        {
            #define SQUARE(x)			( (x)*(x) )
            #define LENGTHSQUARED(x)	( dot( x, x ) )
            float DistSq = LENGTHSQUARED( TopLeftDelta );
            InCurve = ( DistSq > SQUARE(CurveSize) );
            InHighlight = ( DistSq > SQUARE(CurveSize*HighlightCurveMin) && DistSq < SQUARE(CurveSize*HighlightCurveMax) );
        }
        
        vec2 TopRightCurvePivot = vec2(PopsicleRect.z-CurveSize,PopsicleRect.y+CurveSize);
        vec2 TopRightDelta = boxuv - TopRightCurvePivot;
        if ( InsideRectf( boxuv, PopsicleRect.z-CurveSize, 0.0, 1.0, PopsicleRect.y+CurveSize ) )
        {
            float DistSq = LENGTHSQUARED( TopRightDelta );
            InCurve = ( DistSq > SQUARE(CurveSize) );
        }
       

        if ( !InCurve )
	 	   rgba.rgb = OrangeColour;
        if ( InHighlight )
            rgba.rgb = HighlightColour;
    }
    else if ( InsideRect( boxuv, StickShadowRect ) )
    {
 	   rgba.rgb = StickShadowColour;
    }
    else if ( InsideRect( boxuv, StickRect ) )
    {
 	   rgba.rgb = StickColour;
    }


)*###*"

#endif

""

