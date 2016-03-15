R"*###*(


struct PixelInputType
{
    float4 position : SV_POSITION;
    float2 tex : TEXCOORD0;
};

float3 Colour;
float4 BarRect;
float4 Frag(PixelInputType input) : SV_TARGET
{
	float2 uv = input.tex;
	float4 rgba = float4( Colour.x, Colour.y, Colour.z, 1 );
	if ( uv.x < BarRect.x || uv.y < BarRect.y || uv.x > BarRect.x+BarRect.z || uv.y > BarRect.y+BarRect.w )
		discard;
	
	return rgba;
}


)*###*"

