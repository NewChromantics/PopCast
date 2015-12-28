#include "TBlitterDirectx.h"
#include "TBlitterOpengl.h"
#include <SoyMedia.h>



#if (POPMOVIE_WATERMARK!=0)

//	fmod in hlsl isn't exactly the same as GLSL mod()
//	http://stackoverflow.com/a/7614734
//	x - y * trunc(x/y). 

//	grahams fancy chevron watermark; https://www.shadertoy.com/view/4lSXRW
#define APPLY_WATERMARK_HLSL(rgba,uv)	\
	"float stripexval = fmod((1.0-" uv ".y+0.01)*4.0,0.7);\n"	\
	"float stripeyval = fmod((" uv ".x+0.01)*10.0,1.2);\n"	\
	"bool stripexa = stripexval  >= lerp( 0.0, 0.5, stripeyval+0.0 ) && stripexval <= 1.0-lerp( 0.0, 0.5, stripeyval+0.0 ) && stripeyval > 0.7;\n"	\
	"bool stripexb = stripexval  >= lerp( 0.0, 0.5, stripeyval+0.2 ) && stripexval <= 1.0-lerp( 0.0, 0.5, stripeyval+0.2 );\n"	\
	"bool stripexc = stripexval >= lerp( 0.0, 0.5, stripeyval+0.4 ) && stripexval <= 1.0-lerp( 0.0, 0.5, stripeyval+0.4 );\n"	\
	"bool stripexd = stripexval >= lerp( 0.0, 0.5, stripeyval+0.6 ) && stripexval <= 1.0-lerp( 0.0, 0.5, stripeyval+0.6 );\n"	\
	"bool stripex = ( stripexa && !stripexb ) || ( stripexc && !stripexd );\n"	\
	"bool stripey = stripeyval > 0.30;\n"	\
	"if ( stripex && stripey  )\n"	\
	"	" rgba ".rgb = " WATERMARK_RGB_HLSL ";\n"

#else

    #define APPLY_WATERMARK_HLSL(rgba,uv)	"{}"

#endif



const char* Directx::BlitVertShader = 
"struct VertexInputType\n"
"{\n"
"    float2 tex : TEXCOORD0;\n"
"};\n"
"struct PixelInputType\n"
"{\n"
"    float4 position : SV_POSITION;\n"
"    float2 tex : TEXCOORD0;\n"
"};\n"
"PixelInputType Vert(VertexInputType input)\n"
"{\n"
"    PixelInputType output;\n"
"	output.position = float4(input.tex.x,input.tex.y,0,1);\n"
"	output.position.xy *= float2(2,2);\n"
"	output.position.xy -= float2(1,1);\n"
"	output.tex = input.tex;\n"
"    return output;\n"
"}\n"
"";



static auto BlitFragShaderTest = 
"struct PixelInputType\n"
"{\n"
"    float4 position : SV_POSITION;\n"
"    float2 tex : TEXCOORD0;\n"
"};\n"
"float4 Frag(PixelInputType input) : SV_TARGET\n"
"{\n"
"	float4 rgba = float4(input.tex.x,input.tex.y,0,1);\n"
APPLY_WATERMARK_HLSL("rgba","input.tex")
"    return rgba;\n"
"}\n"
"";

static auto BlitFragShaderError = 
"struct PixelInputType\n"
"{\n"
"    float4 position : SV_POSITION;\n"
"    float2 tex : TEXCOORD0;\n"
"};\n"
"float4 Frag(PixelInputType input) : SV_TARGET\n"
"{\n"
"	float4 rgba = float4(1,0.3*input.tex.x,0.3*input.tex.y,1);\n"
APPLY_WATERMARK_HLSL("rgba","input.tex")
"	return rgba;\n"
"}\n"
"";


static auto BlitFragShaderRgba = 
"Texture2D Texture0;\n"
"SamplerState Texture0Sampler;\n"
"struct PixelInputType\n"
"{\n"
"	float4 position : SV_POSITION;\n"
"	float2 tex : TEXCOORD0;\n"
"};\n"
"float4 Frag(PixelInputType input) : SV_TARGET\n"
"{\n"
"	float4 rgba = Texture0.Sample( Texture0Sampler, input.tex );\n"
APPLY_WATERMARK_HLSL("rgba","input.tex")
"	rgba.w=1;\n"
"	return rgba;\n"
"}\n"
"";



static std::string BlitFragShaderYuv(const Soy::TYuvParams& Yuv)
{
	std::stringstream Shader;
	Shader <<
	//"varying " PREC " vec2 oTexCoord;\n"
	//"uniform sampler2D Texture0;\n"
	//"uniform sampler2D Texture1;\n"
	//"const " PREC "mat3 Transform = mat3(	1,0,0,	0,1,0,	0,0,1	);\n";
	//"uniform " PREC "mat3 Transform;\n";
	"Texture2D Texture0;\n"
	"SamplerState Texture0Sampler;\n"
	"Texture2D Texture1;\n"
	"SamplerState Texture1Sampler;\n"
	;

	//	gles compiler won't accept 1 or 0 as a float. force decimal places
	Shader << "static const float LumaMin = " << std::fixed << Yuv.mLumaMin << ";\n";
	Shader << "static const float LumaMax = " << std::fixed << Yuv.mLumaMax << ";\n";
	Shader << "static const float ChromaVRed = " << std::fixed << Yuv.mChromaVRed << ";\n";
	Shader << "static const float ChromaUGreen = " << std::fixed << Yuv.mChromaUGreen << ";\n";
	Shader << "static const float ChromaVGreen = " << std::fixed << Yuv.mChromaVGreen << ";\n";
	Shader << "static const float ChromaUBlue = " << std::fixed << Yuv.mChromaUBlue << ";\n";
	
	Shader <<
	"struct PixelInputType\n"
	"{\n"
	"	float4 position : SV_POSITION;\n"
	"	float2 tex : TEXCOORD0;\n"
	"};\n"
	;

	Shader <<
	"float4 Frag(PixelInputType input) : SV_TARGET\n"
	"{\n"
	//"	float2 uv = (vec3(oTexCoord.x,oTexCoord.y,1)*Transform).xy;\n"
	"	float2 uv = input.tex;\n"
	//	scale video luma to to 0..1
	"	float Luma = Texture0.Sample(Texture0Sampler,uv).x;\n"
	"	Luma = (Luma-LumaMin) / (LumaMax-LumaMin);\n"
	//	convert chroma from 0..1 to -0.5..0.5
	"	float2 ChromaUv = Texture1.Sample(Texture1Sampler,uv).xy;\n"
	"	ChromaUv -= float2(0.5,0.5);\n"
	//	convert to rgb
	"	float4 rgba;\n"
	"	rgba.r = Luma + ChromaVRed * ChromaUv.y;\n"
	"	rgba.g = Luma + ChromaUGreen * ChromaUv.x + ChromaVGreen * ChromaUv.y;\n"
	"	rgba.b = Luma + ChromaUBlue * ChromaUv.x;\n"
	"	rgba.a = 1.0;\n"
	APPLY_WATERMARK_HLSL("rgba","uv")
	"	return rgba;\n"
	"}\n";
	return Shader.str();
};


std::shared_ptr<Directx::TGeometry> Directx::TBlitter::GetGeo(Directx::TContext& Context)
{
	if ( mBlitQuad )
		return mBlitQuad;
	
	//	for each part of the vertex, add an attribute to describe the overall vertex
	Opengl::TGeometryVertex Vertex;
	Array<uint8> MeshData;
	Array<size_t> Indexes;
	Soy::TBlitter::GetGeo( Vertex, GetArrayBridge(MeshData), GetArrayBridge(Indexes), true );

	mBlitQuad.reset( new Directx::TGeometry( GetArrayBridge(MeshData), GetArrayBridge(Indexes), Vertex, Context ) );
	return mBlitQuad;
}


std::shared_ptr<Directx::TShader> AllocShader(std::shared_ptr<Directx::TShader>& Shader,const std::string& Name,const std::string& VertShader,const std::string& FragShader,Opengl::TGeometryVertex& VertexDescription,std::shared_ptr<Directx::TShader> BackupShader,Directx::TContext& Context)
{
	if ( Shader )
		return Shader;
	
	try
	{
		Shader.reset( new Directx::TShader( VertShader, FragShader, VertexDescription, Name, Context ) );
	}
	catch( std::exception& e )
	{
		std::Debug << "Failed to allocate shader: " << e.what() << std::endl;
		return BackupShader;
	}
	
	
	return Shader;
}


std::shared_ptr<Directx::TShader> Directx::TBlitter::GetTestShader(Directx::TContext& Context)
{
	auto BlitGeo = GetGeo( Context );
	if ( !BlitGeo )
	{
		throw Soy::AssertException("Cannot allocate shader without geometry");
		return nullptr;
	}
	auto& VertexDesc = BlitGeo->mVertexDescription;
	
	
	//	make sure the test shader is allocated and return it if any others failed
	if ( !mBlitShaderTest )
	{
		AllocShader( mBlitShaderTest, "Test", BlitVertShader, BlitFragShaderTest, VertexDesc, nullptr, Context );
	}

	return mBlitShaderTest;
}


std::shared_ptr<Directx::TShader> Directx::TBlitter::GetErrorShader(Directx::TContext& Context)
{
	auto BlitGeo = GetGeo( Context );
	if ( !BlitGeo )
	{
		throw Soy::AssertException("Cannot allocate shader without geometry");
		return nullptr;
	}
	auto& VertexDesc = BlitGeo->mVertexDescription;
	
	if ( !mBlitShaderError )
	{
		AllocShader( mBlitShaderError, "Error", BlitVertShader, BlitFragShaderError, VertexDesc, nullptr, Context );
	}
	
	return mBlitShaderError;
}


std::shared_ptr<Directx::TShader> Directx::TBlitter::GetShader(ArrayBridge<SoyPixelsImpl*>& Sources,Directx::TContext& Context)
{
	auto BlitGeo = GetGeo( Context );
	if ( !BlitGeo )
	{
		throw Soy::AssertException("Cannot allocate shader without geometry");
		return nullptr;
	}
	auto& VertexDesc = BlitGeo->mVertexDescription;
	
	
	//	make sure the test shader is allocated and return it if any others failed
	if ( !mBlitShaderTest )
	{
		AllocShader( mBlitShaderTest, "Test", BlitVertShader, BlitFragShaderTest, VertexDesc, nullptr, Context );
	}
	
	if ( !mBlitShaderError )
	{
		AllocShader( mBlitShaderError, "Error", BlitVertShader, BlitFragShaderError, VertexDesc, nullptr, Context );
	}
	
	if ( Sources.GetSize() == 0 )
		return mBlitShaderTest;
	
	//	alloc and return the shader we need
	if ( Sources.GetSize() > 1 && Sources[0]!=nullptr )
	{
		auto& Texture0 = *Sources[0];
		if ( Texture0.GetMeta().GetFormat() == SoyPixelsFormat::LumaVideo )
			return AllocShader( mBlitShaderYuvVideo, "Yuv Video", BlitVertShader, BlitFragShaderYuv(Soy::TYuvParams::Video()), VertexDesc, mBlitShaderError, Context  );
		
		//if ( Texture0.mMeta.GetFormat() == SoyPixelsFormat::LumaFull )
		return AllocShader( mBlitShaderYuvFull, "Yuv Full", BlitVertShader, BlitFragShaderYuv(Soy::TYuvParams::Full()), VertexDesc, mBlitShaderError, Context  );
	}


	auto& Texture0 = Sources[0];
	
	/*
	//	alloc and return the shader we need
	if ( Sources.GetSize() > 1 )
	{
		if ( Texture0.mMeta.GetFormat() == SoyPixelsFormat::LumaVideo )
			return AllocShader( mBlitShaderYuvVideo, "Yuv", BlitVertShader, BlitFragShaderYuv_Video(), VertexDesc, mBlitShaderTest, Context  );
		
		//if ( Texture0.mMeta.GetFormat() == SoyPixelsFormat::LumaFull )
		return AllocShader( mBlitShaderYuvFull, "Yuv", BlitVertShader, BlitFragShaderYuv_Full(), VertexDesc, mBlitShaderTest, Context  );
	}
	*/

	if ( Sources.GetSize() > 0 )
	{
		return AllocShader( mBlitShaderRgba, "Rgba", BlitVertShader, BlitFragShaderRgba, VertexDesc, nullptr, Context );
	}
	
	//	don't know which shader to use!
	std::Debug << "Don't know what blit shader to use for texture[s] x" << Sources.GetSize() << std::endl;
	return mBlitShaderError;
}


void Directx::TBlitter::BlitError(Directx::TTexture& Target,const std::string& Error,Directx::TContext& Context)
{
	//	gr: add non-
	std::shared_ptr<Directx::TShader> ErrorShader = GetErrorShader( Context );

	BufferArray<SoyPixelsImpl*,2> Textures;
	BlitTexture( Target, GetArrayBridge(Textures), Context, ErrorShader );
}

void Directx::TBlitter::BlitTexture(Directx::TTexture& Target, ArrayBridge<SoyPixelsImpl*>&& Sources,TContext& Context,std::shared_ptr<Directx::TShader> OverrideShader)
{
	if ( mUseTestBlit && !OverrideShader )
		OverrideShader = GetTestShader( Context );

	Array<std::shared_ptr<Directx::TTexture>> SourceTextures;
	for ( int s=0;	s<Sources.GetSize();	s++ )
	{
		//	copy pixels to a dynamic texture and then copy that to target
		Soy::Assert( Sources[s] != nullptr, "Expected non-null pixels");
		auto& Pixels = *Sources[s];
		
		//	allocate a dynamic texture to put pixels into
		auto PixelTexture = GetTempTexturePtr( Pixels.GetMeta(), Context, TTextureMode::Writable );

		std::stringstream Error;
		Error << "Failed to allocate temp texture " << Pixels.GetMeta();
		Soy::Assert( PixelTexture!=nullptr, Error.str() );
		PixelTexture->Write( Pixels, Context );
		SourceTextures.PushBack( PixelTexture );
	}

	//	now create a rendertarget texture we can blit that texture[s] to the final target
	//	todo: if target is a render texture, blit directly into that instead of an intermediate temp texture
	auto RenderTexture = GetTempTexturePtr( Target.mMeta, Context, TTextureMode::RenderTarget );

	//	blit pixels into it
	auto& RenderTarget = GetRenderTarget( RenderTexture, Context );
	RenderTarget.Bind( Context );
	RenderTarget.ClearColour( Context, Soy::TRgb(0,1,0) );

	auto BlitGeo = GetGeo( Context );
	auto BlitShader = OverrideShader ? OverrideShader : GetShader( Sources, Context );
	
	Soy::Assert( BlitGeo!=nullptr, "Failed to get geo for blit");
	Soy::Assert( BlitShader!=nullptr,"Failed to get shader for blit");

	{
		auto Shader = BlitShader->Bind( Context );
		try
		{
			static bool setuniforms = true;
			if (setuniforms)
			{
				Shader.SetUniform("Transform", mTransform );
				//	try to set all the uniforms
				//	assume pixel size might fail, not all shaders need it
				const char* TextureUniforms[] = { "Texture0", "Texture1" };
				//const char* TextureSizeUniforms[] = { "Texture0_PixelSize", "Texture1_PixelSize" };
				for (int t = 0; t < SourceTextures.GetSize(); t++)
				{
					Shader.SetUniform(TextureUniforms[t], *SourceTextures[t] );
					//vec2f PixelSize(Sources[t].GetWidth(), Sources[t].GetHeight());
					//Shader.SetUniform(TextureSizeUniforms[t], PixelSize);
				}
			}
			Shader.Bake();
			BlitGeo->Draw( Context );
		}
		catch(std::exception& e)
		{
			std::Debug << "Geo blit error: " << e.what() << std::endl;
			RenderTarget.ClearColour( Context, Soy::TRgb(1,0,0) );
		}
	}
	RenderTarget.Unbind(Context);

	//	now copy that render target to texture target
	Target.Write( *RenderTexture, Context );
}


std::shared_ptr<Directx::TTexture> Directx::TBlitter::GetTempTexturePtr(SoyPixelsMeta Meta,TContext& Context,TTextureMode::Type Mode)
{
	//	already have one?
	for ( int t=0;	t<mTempTextures.GetSize();	t++ )
	{
		auto pTexture = mTempTextures[t];
		if ( !pTexture )
			continue;

		if ( pTexture->mMeta != Meta )
			continue;

		if ( pTexture->GetMode() != Mode )
			continue;

		return pTexture;
	}

	//	no matching ones, allocate a new one
	std::shared_ptr<TTexture> Texture( new TTexture( Meta, Context, Mode ) );
	mTempTextures.PushBack( Texture );
	return Texture;
}


Directx::TTexture& Directx::TBlitter::GetTempTexture(SoyPixelsMeta Meta,TContext& Context,TTextureMode::Type Mode)
{
	auto pTexture = GetTempTexturePtr( Meta, Context, Mode );
	return *pTexture;
}

Directx::TRenderTarget& Directx::TBlitter::GetRenderTarget(std::shared_ptr<TTexture>& Texture,TContext& Context)
{
	//	already have one?
	for ( int t=0;	t<mRenderTargets.GetSize();	t++ )
	{
		auto pRenderTarget = mRenderTargets[t];
		if ( !pRenderTarget )
			continue;

		if ( *pRenderTarget != *Texture )
			continue;

		return *pRenderTarget;
	}

	//	no matching ones, allocate a new one
	std::shared_ptr<TRenderTarget> RenderTarget( new TRenderTarget( *Texture, Context ) );
	mRenderTargets.PushBack( RenderTarget );
	return *RenderTarget;
}


