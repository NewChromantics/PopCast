R"*###*(



struct PixelInputType
{
    float4 position : SV_POSITION;
    float2 tex : TEXCOORD0;
};

float4 Frag(PixelInputType input) : SV_TARGET
{
	float2 uv = input.tex;

	return float4( uv.y, 0, 0, 1 );	//	only x matters
}



)*###*"