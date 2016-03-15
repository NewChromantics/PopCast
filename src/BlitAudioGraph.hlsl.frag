R"*###*(


struct PixelInputType
{
    float4 position : SV_POSITION;
    float2 tex : TEXCOORD0;
};

float4 Frag(PixelInputType input) : SV_TARGET
{
	float2 uv = input.tex;
	float4 rgba = float4( uv.x, uv.y, 1, 1 );
	return rgba;
}


)*###*"

