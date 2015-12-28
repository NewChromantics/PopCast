#pragma once


//	gl es 2 requires precision specification. todo; add this to the shader upgrader if it's the first thing on the line
//	or a varying

//	ES2 (what glsl?) requires precision, but not on mat?
//	ES3 glsl 100 (android s6) requires precision on vec's and mat
//	gr: changed to highprecision to fix sampling aliasing with the bjork video
#define PREC	"highp "


 
#if (POPMOVIE_WATERMARK!=0)
 
 
#define WATERMARK_PREAMBLE_GLSL	\
"#define BIT_MAX 16\n"	\
"uniform vec2 Texture0_PixelSize;\n"	\
"int Watermark_00 = 0x7FFF;	int Watermark_10 = 0xFFFC;	"\
"int Watermark_01 = 0xC444; int Watermark_11 = 0x4446;	"\
"int Watermark_02 = 0xD555;	int Watermark_12 = 0xD5EE;	"\
"int Watermark_03 = 0xC545;	int Watermark_13 = 0xC46E;	"\
"int Watermark_04 = 0xDD5D;	int Watermark_14 = 0xD76E;	"\
"int Watermark_05 = 0xDC5C;	int Watermark_15 = 0x546E;	"\
"int Watermark_06 = 0x7FFF;	int Watermark_16 = 0xFFFC;	"\
"int Watermark_07 = 0xE00;	int Watermark_17 = 0;	"\
"int Watermark_08 = 0x600;	int Watermark_18 = 0;	"\
"int Watermark_09 = 0x300;	int Watermark_19 = 0;"	\
"int bitwiseAnd(int a, int bit)	{"	\
"	if ( bit > BIT_MAX ) return 0;"	\
"	bit = BIT_MAX-bit;"	\
"	int b = 1;"	\
"	for ( int i=0;i<BIT_MAX;i++ )"	\
"		if ( bit > 0 )"	\
"		{	b *= 2;	bit -= 1;	}"	\
"	int result = 0;"	\
"	int n = 1;"	\
"	for ( int i=0; i<BIT_MAX; i++ ){"	\
"		if ((a > 0) && (b > 0)) {"	\
"			if (((mod(float(a),2.0)) == 1.0) && ((mod(float(b),2.0)) == 1.0))"	\
"				result += n;"	\
"			a = a / 2;"	\
"			b = b / 2;"	\
"			n = n * 2;"	\
"		}"	\
"	 else break;"	\
"	}"	\
"	return result;"	\
"}"	\
"int GetWatermark(int x, int y){"	\
"	if ( y == 0 )	return x <= 16 ? Watermark_00 : Watermark_10;"	\
"	if ( y == 1 )	return x <= 16 ? Watermark_01 : Watermark_11;"	\
"	if ( y == 2 )	return x <= 16 ? Watermark_02 : Watermark_12;"	\
"	if ( y == 3 )	return x <= 16 ? Watermark_03 : Watermark_13;"	\
"	if ( y == 4 )	return x <= 16 ? Watermark_04 : Watermark_14;"	\
"	if ( y == 5 )	return x <= 16 ? Watermark_05 : Watermark_15;"	\
"	if ( y == 6 )	return x <= 16 ? Watermark_06 : Watermark_16;"	\
"	if ( y == 7 )	return x <= 16 ? Watermark_07 : Watermark_17;"	\
"	if ( y == 8 )	return x <= 16 ? Watermark_08 : Watermark_18;"	\
"	if ( y == 9 )	return x <= 16 ? Watermark_09 : Watermark_19;"	\
"	return 0;"	\
"}"	\

#define WATERMARK_WIDTH	"10.0"	//	percent
#define WATERMARK_X		"4"
#define WATERMARK_Y		"3"

#define APPLY_WATERMARK_GLSL(rgba,uv)	\
"int x = int(floor(" uv ".x * (100.0-" WATERMARK_WIDTH ")) )- " WATERMARK_X ";\n"	\
"int y = int((1.0-" uv ".y) * (100.0-" WATERMARK_WIDTH ") * (Texture0_PixelSize.y/Texture0_PixelSize.x)) - " WATERMARK_Y ";\n"	\
"int Watermark = GetWatermark( x, y );"	\
"bool WatermarkPixel = bitwiseAnd( Watermark, x<=16 ? x : x-16 ) != 0;"	\
"if ( WatermarkPixel )	" rgba ".rgb = vec3( 1,1,1 );"\


#else

#define APPLY_WATERMARK_GLSL(rgba,uv)	"{}"

#endif

