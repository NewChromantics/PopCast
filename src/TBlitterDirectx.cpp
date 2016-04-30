#include "TBlitterDirectx.h"
#include <SoyMedia.h>
#include <SoyJson.h>


const char* Directx::BlitVertShader = 
#include "Blit.hlsl.vert"
;

static auto BlitFragShaderTest =
#include "BlitTest.hlsl.frag"
;

static auto BlitFragShaderError = 
#include "BlitTest.hlsl.frag"
;

static auto BlitFragShaderRgba = 
#include "BlitRgba.hlsl.frag"
;

const char* WatermarkHlsl =
#include "BlitWatermark.hlsl.frag"
;


static auto BlitFragShader_Yuv_8_88_Full =
#include "BlitYuv_8_88_Full.hlsl.frag"
;
static auto BlitFragShader_Yuv_8_88_Ntsc =
#include "BlitYuv_8_88_Ntsc.hlsl.frag"
;
static auto BlitFragShader_Yuv_8_88_Smptec =
#include "BlitYuv_8_88_Ntsc.hlsl.frag"
;



static auto BlitFragShader_Yuv_8_8_8_Full =
#include "BlitYuv_8_8_8_Full.hlsl.frag"
;
static auto BlitFragShader_Yuv_8_8_8_Ntsc =
#include "BlitYuv_8_8_8_Ntsc.hlsl.frag"
;
static auto BlitFragShader_Yuv_8_8_8_Smptec =
#include "BlitYuv_8_8_8_Ntsc.hlsl.frag"
;


static auto BlitFragShader_Yuv_844_Full =
#include "BlitYuv_844_Ntsc.hlsl.frag"
;
static auto BlitFragShader_Yuv_844_Ntsc =
#include "BlitYuv_844_Ntsc.hlsl.frag"
;
static auto BlitFragShader_Yuv_844_Smptec =
#include "BlitYuv_844_Ntsc.hlsl.frag"
;


Directx::TBlitter::TBlitter(std::shared_ptr<TPool<TTexture>> TexturePool) :
	mTempTextures	( TexturePool )
{
	if ( !mTempTextures )
		mTempTextures.reset( new TPool<TTexture>() );
}

TPool<Directx::TTexture>& Directx::TBlitter::GetTexturePool()
{
	return *mTempTextures;
}


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



std::shared_ptr<Directx::TShader> Directx::TBlitter::GetShader(const std::string& Name,const char* Source,TContext& Context)
{
	//	see if it's already allocated
	{
		auto ShaderIt = mBlitShaders.find( Source );
		if ( ShaderIt != mBlitShaders.end() )
		{
			Soy::Assert( ShaderIt->second != nullptr, "Null shader in blit list");
			return ShaderIt->second;
		}
	}

	//	get the stuff we need to allocate
	auto BlitGeo = GetGeo( Context );
	if ( !BlitGeo )
	{
		throw Soy::AssertException("Cannot allocate shader without geometry");
		return nullptr;
	}
	auto& VertexDesc = BlitGeo->mVertexDescription;

	try
	{
		auto pShader = std::make_shared<Directx::TShader>( BlitVertShader, Source, VertexDesc, Name, Context );
		mBlitShaders[Source] = pShader;
		return pShader;
	}
	catch(std::exception& e)
	{
		//	don't recurse
		if ( Source == BlitFragShaderTest )
		{
			std::Debug << "Failed to allocate test shader(" << Name << "): " << e.what() << std::endl;
			return nullptr;
		}

		std::Debug << "Failed to allocate shader " << Name << ", reverting to test shader... Exception: " << e.what() << std::endl;
		return GetBackupShader( Context );
	}
}



std::shared_ptr<Directx::TShader> Directx::TBlitter::GetBackupShader(TContext& Context)
{
	return GetShader("Backup", BlitFragShaderTest, Context );
}

std::shared_ptr<Directx::TShader> Directx::TBlitter::GetErrorShader(TContext& Context)
{
	return GetShader("Error", BlitFragShaderError, Context );
}


std::shared_ptr<Directx::TShader> Directx::TBlitter::GetShader(ArrayBridge<std::shared_ptr<Directx::TTexture>>& Sources,Directx::TContext& Context)
{
	BufferArray<Directx::TTexture,10> SourcesNoPtr;
	for ( int i=0;	i<Sources.GetSize();	i++ )
	{
		SourcesNoPtr.PushBack( *Sources[i] );
	}
	return GetShader( GetArrayBridge(SourcesNoPtr), Context );
}

std::shared_ptr<Directx::TShader> Directx::TBlitter::GetShader(ArrayBridge<Directx::TTexture>& Sources,Directx::TContext& Context)
{
	if ( Sources.GetSize() == 0 )
		return GetBackupShader(Context);
	
	auto& Texture0 = Sources[0];
	
	//	alloc and return the shader we need
	if ( Sources.GetSize() > 1  && mMergeYuv )
	{
		auto& Texture1 = Sources[1];
		auto MergedFormat = SoyPixelsFormat::GetMergedFormat( Texture0.GetMeta().GetFormat(), Texture1.GetMeta().GetFormat() );

		switch ( MergedFormat )
		{
			case SoyPixelsFormat::Yuv_8_88_Full:	return GetShader( SoyPixelsFormat::ToString(MergedFormat), BlitFragShader_Yuv_8_88_Full, Context );
			case SoyPixelsFormat::Yuv_8_88_Ntsc:	return GetShader( SoyPixelsFormat::ToString(MergedFormat), BlitFragShader_Yuv_8_88_Ntsc, Context );
			case SoyPixelsFormat::Yuv_8_88_Smptec:	return GetShader( SoyPixelsFormat::ToString(MergedFormat), BlitFragShader_Yuv_8_88_Smptec, Context );
	
			case SoyPixelsFormat::Yuv_8_8_8_Full:	return GetShader( SoyPixelsFormat::ToString(MergedFormat), BlitFragShader_Yuv_8_8_8_Full, Context );
			case SoyPixelsFormat::Yuv_8_8_8_Ntsc:	return GetShader( SoyPixelsFormat::ToString(MergedFormat), BlitFragShader_Yuv_8_8_8_Ntsc, Context );
			case SoyPixelsFormat::Yuv_8_8_8_Smptec:	return GetShader( SoyPixelsFormat::ToString(MergedFormat), BlitFragShader_Yuv_8_8_8_Smptec, Context );
		}

		std::Debug << "Warning: No frag shader specified for merged format " << MergedFormat << " (" << Texture0.GetMeta().GetFormat() << " + " << Texture1.GetMeta().GetFormat() << ")" << std::endl;
	}

	//	special formats
	if ( Sources.GetSize() == 1 )
	{
		//	if the directx format is YUY2 the RGBA hardware filtering does the conversion for us
		if ( Texture0.GetDirectxFormat() == DXGI_FORMAT_YUY2 )
		{
			return GetShader( "Rgba", BlitFragShaderRgba, Context  );
		}

		//	special YUY2 format
		if ( Texture0.GetDirectxFormat() == DXGI_FORMAT_R8G8_B8G8_UNORM )
		{
			return GetShader( "DXGI_FORMAT_R8G8_B8G8", BlitFragShader_Yuv_844_Full, Context  );
			//return GetShader( "Rgba", BlitFragShaderRgba, Context  );
		}
		
		//	gr: different formats wanted for different texture types really...
		//	gr: these are only required if the hardware doesn't support the YUY2 format (win8... check the target texture format?)
		/*
		switch ( Format0 )
		{
			case SoyPixelsFormat::Yuv_844_Full:		return GetShader( SoyPixelsFormat::ToString(Format0), BlitFragShader_Yuv_844_Full, Context  );
			case SoyPixelsFormat::Yuv_844_Ntsc:		return GetShader( SoyPixelsFormat::ToString(Format0), BlitFragShader_Yuv_844_Ntsc, Context  );
			case SoyPixelsFormat::Yuv_844_Smptec:	return GetShader( SoyPixelsFormat::ToString(Format0), BlitFragShader_Yuv_844_Smptec, Context  );
		}
		*/
		auto Format0 = Texture0.GetMeta().GetFormat();
		switch ( Format0 )
		{
			case SoyPixelsFormat::YYuv_8888_Full:	return GetShader( SoyPixelsFormat::ToString(Format0), BlitFragShader_Yuv_844_Full, Context  );
			case SoyPixelsFormat::YYuv_8888_Ntsc:	return GetShader( SoyPixelsFormat::ToString(Format0), BlitFragShader_Yuv_844_Ntsc, Context  );
			case SoyPixelsFormat::YYuv_8888_Smptec:	return GetShader( SoyPixelsFormat::ToString(Format0), BlitFragShader_Yuv_844_Smptec, Context  );
		}
	}

	return GetShader( "Rgba", BlitFragShaderRgba, Context  );
}



void Directx::TBlitter::BlitError(Directx::TTexture& Target,const std::string& Error,Directx::TContext& Context)
{
	//	gr: until we have text blitting, we need to at least print out the error
	std::Debug << "BlitError(" << Error << ")" << std::endl;

	//	gr: add non-
	std::shared_ptr<Directx::TShader> ErrorShader = GetErrorShader( Context );

	BufferArray<const SoyPixelsImpl*,2> Textures;
	BlitTexture( Target, GetArrayBridge(Textures), Context, ErrorShader );
}


void Directx::TBlitter::BlitTexture(Directx::TTexture& Target, ArrayBridge<const SoyPixelsImpl*>&& Sources,TContext& Context,const char* OverrideShader)
{
	//	grab provided shader if it's already compiled
	std::shared_ptr<TShader> OverrideShaderPtr;
	if ( OverrideShader )
	{
		OverrideShaderPtr = GetShader( "OverrideShader", OverrideShader, Context );
	}
	
	BlitTexture( Target, GetArrayBridge(Sources), Context, OverrideShaderPtr );
}

void Directx::TBlitter::BlitTexture(Directx::TTexture& Target, ArrayBridge<const SoyPixelsImpl*>&& Sources,TContext& Context,std::shared_ptr<Directx::TShader> OverrideShader)
{
	if ( mUseTestBlit && !OverrideShader )
		OverrideShader = GetBackupShader( Context );

	Array<std::shared_ptr<Directx::TTexture>> SourcePoolTextures;
	Array<Directx::TTexture> SourceTextures;
	for ( int s=0;	s<Sources.GetSize();	s++ )
	{
		//	copy pixels to a dynamic texture and then copy that to target
		Soy::Assert( Sources[s] != nullptr, "Expected non-null pixels");
		auto& Pixels = *Sources[s];

		//	allocate a dynamic texture to put pixels into
		auto PixelTexture = GetTempTexturePtr( Pixels.GetMeta(), Context, TTextureMode::WriteOnly );

		std::stringstream Error;
		Error << "Failed to allocate temp texture " << Pixels.GetMeta();
		Soy::Assert( PixelTexture!=nullptr, Error.str() );
		PixelTexture->Write( Pixels, Context );
		SourcePoolTextures.PushBack( PixelTexture );
		SourceTextures.PushBack( *PixelTexture );
	}

	auto& TexturePool = GetTexturePool();
	for ( int i=0;	i<SourceTextures.GetSize();	i++ )
	{
		TexturePool.Release(SourcePoolTextures[i]);
	}
}


void Directx::TBlitter::BlitTexture(Directx::TTexture& Target,ArrayBridge<Directx::TTexture>&& Sources,TContext& Context,const char* OverrideShader)
{
	//	grab provided shader if it's already compiled
	std::shared_ptr<TShader> OverrideShaderPtr;
	if ( OverrideShader )
	{
		OverrideShaderPtr = GetShader( "OverrideShader", OverrideShader, Context );
	}

	BlitTexture( Target, GetArrayBridge(Sources), Context, OverrideShaderPtr );
}

void Directx::TBlitter::BlitTexture(Directx::TTexture& Target,ArrayBridge<Directx::TTexture>&& OrigSourceTextures,TContext& Context,std::shared_ptr<Directx::TShader> OverrideShader)
{
	if ( mUseTestBlit && !OverrideShader )
		OverrideShader = GetBackupShader( Context );

	Array<std::shared_ptr<TTexture>> TempTextures;

	//	if sources are X, we cannot bind them as input, so need to be copied
	Array<Directx::TTexture> SourceTextures;
	for ( int t=0;	t<OrigSourceTextures.GetSize();	t++ )
	{
		auto& OrigSource = OrigSourceTextures[t];
		
		if ( OrigSource.CanBindToShaderUniform() )
		{
			SourceTextures.PushBack( OrigSource );
		}
		else
		{
			//	copy
			auto OrigSourceCopy = GetTempTexturePtr( OrigSource.mMeta, Context, TTextureMode::GpuOnly );
			TempTextures.PushBack( OrigSourceCopy );
			OrigSourceCopy->Write( OrigSource, Context );
			SourceTextures.PushBack( *OrigSourceCopy );
		}
	}

	//	now create a rendertarget texture we can blit that texture[s] to the final target
	//	todo: if target is a render texture, blit directly into that instead of an intermediate temp texture
	auto RenderTexture = GetTempTexturePtr( Target.mMeta, Context, TTextureMode::RenderTarget );
	TempTextures.PushBack( RenderTexture );

	auto Dealloc = [this,&TempTextures]
	{
		auto& TexturePool = GetTexturePool();
		for ( int i=0;	i<TempTextures.GetSize();	i++ )
			TexturePool.Release( TempTextures[i] );
	};

	try
	{
		//	blit pixels into it
		auto& RenderTarget = GetRenderTarget( RenderTexture, Context );
		RenderTarget.Bind( Context );
		RenderTarget.ClearColour( Context, Soy::TRgb(0,1,0) );

		auto BlitGeo = GetGeo( Context );
		auto BlitShader = OverrideShader ? OverrideShader : GetShader( GetArrayBridge(SourceTextures), Context );

		Soy::Assert( BlitGeo!=nullptr, "Failed to get geo for blit");
		Soy::Assert( BlitShader!=nullptr,"Failed to get shader for blit");

		{
			try
			{
				auto Shader = BlitShader->Bind( Context );
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
						Shader.SetUniform(TextureUniforms[t], SourceTextures[t] );
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
		Dealloc();
	}
	catch(...)
	{
		Dealloc();
		throw;
	}
}


std::shared_ptr<Directx::TTexture> Directx::TBlitter::GetTempTexturePtr(SoyPixelsMeta Meta,TContext& Context,TTextureMode::Type Mode)
{
	auto RealAlloc = [&Meta,&Mode,&Context]()
	{
		return std::make_shared<TTexture>( Meta, Context, Mode );
	};

	auto& TexturePool = GetTexturePool();
	auto Texture = TexturePool.AllocPtr( TTextureMeta(Meta,Mode), RealAlloc );
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


void Directx::TBlitter::GetMeta(TJsonWriter& Json)
{
	Soy::TBlitter::GetMeta( Json );

	auto& Pool = GetTexturePool();

	Json.Push("BlitterTexturePoolSize", Pool.GetAllocCount() );
	Json.Push("BlitterRenderTargetPoolSize", mRenderTargets.GetSize() );
}

