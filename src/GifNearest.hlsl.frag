WATERMARK_PREAMBLE_HLSL


R"*###*(


//	rgba source
Texture2D Texture0;
SamplerState Texture0Sampler;

//	palette
Texture2D Texture1;
SamplerState Texture1Sampler;
//float2 Texture1_PixelSize;

static const int TransparentPixelIndex = 0;


struct PixelInputType
{
    float4 position : SV_POSITION;
    float2 tex : TEXCOORD0;
};


float GetColourScore(float4 a,float4 b)
{
	float3 Diff = abs(b.xyz - a.xyz);
	float Score = 1.0 - ( ( Diff.x + Diff.y + Diff.z) / 3.0f );
	return Score;
}


float4 Frag(PixelInputType input) : SV_TARGET
{
	float2 Texture1_PixelSize = float2( 256.f, 1.0f );

	float2 uv = input.tex;
	float4 rgba = Texture0.Sample( Texture0Sampler, uv );

	#define CORRECT_UV(v)	(1.0-v)

	//	apply watermark here where we read from the source
)*###*"

#include "BlitWatermark.hlsl.frag"

R"*###*(


	float NearestScore = -1;
	int NearestIndex = 1;
	
	for ( float i=0;	i<Texture1_PixelSize.x;	i++ )
	{
		float4 PalColour = Texture1.Sample( Texture1Sampler, float2( i / Texture1_PixelSize.x, 0 )  );
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
	
	//	note: hlsl greyscale requires .w for data and seems to scale or average the others
	rgba = float4( 1, 1, 1, NearestIndexf );
	return rgba;
}



)*###*"
