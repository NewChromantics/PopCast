R"*###*(

struct VertexInputType
{
    float2 tex : TEXCOORD0;
};
struct PixelInputType
{
    float4 position : SV_POSITION;
   float2 tex : TEXCOORD0;
};

PixelInputType Vert(VertexInputType input)
{
    PixelInputType output;
	output.position = float4(input.tex.x,input.tex.y,0,1);
	output.position.xy *= float2(2,2);
	output.position.xy -= float2(1,1);
	output.tex = input.tex;
    return output;
}



)*###*"
