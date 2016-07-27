#include "TBlitterMetal.h"
#include "TBlitter.h"
#include <SoyMedia.h>
#include <SoyJson.h>

#include <SoyGraphics.h>


#if !defined(ENABLE_METAL)
#error metal not enabled
#endif

namespace Renderer = Metal;


namespace PopMovie
{
	Soy::TRgb	AllocClearColour = Soy::TRgb(1,0,1);
	Soy::TRgb	BlitClearColour = Soy::TRgb(1,1,0);
}


const char* BlitVertShader =
#include "Blit.glsl.vert"
;

static auto BlitFragShader2D =
#include "Blit2D.glsl.frag"
;

const char* WatermarkGlsl =
#include "BlitWatermark.glsl.frag"
;

static auto BlitFragShader_Yuv_8_88_Full =
#include "BlitYuv_8_88_Full.glsl.frag"
;
static auto BlitFragShader_Yuv_8_88_Ntsc =
#include "BlitYuv_8_88_Ntsc.glsl.frag"
;
static auto BlitFragShader_Yuv_8_88_Smptec =
#include "BlitYuv_8_88_Ntsc.glsl.frag"
;


static auto BlitFragShader_Yuv_8_8_8_Full =
#include "BlitYuv_8_8_8_Full.glsl.frag"
;
static auto BlitFragShader_Yuv_8_8_8_Ntsc =
#include "BlitYuv_8_8_8_Ntsc.glsl.frag"
;
static auto BlitFragShader_Yuv_8_8_8_Smptec =
#include "BlitYuv_8_8_8_Ntsc.glsl.frag"
;

/*
static auto BlitFragShader_Yuv_844_Full =
#include "BlitYuv_844_Full.glsl.frag"
;
static auto BlitFragShader_Yuv_844_Ntsc =
#include "BlitYuv_844_Ntsc.glsl.frag"
;
static auto BlitFragShader_Yuv_844_Smptec =
#include "BlitYuv_844_Ntsc.glsl.frag"
;
*/

static auto BlitFragShaderRectangle __unused =
#include "BlitRectangle.glsl.frag"
;


//	While the OpenGL ES3 API is backwards compatible with the OpenGL ES2 API, and it supports both GLSL 1.0 and 3.0, GLSL 3.0 isn't defined to be backwards compatible with GLSL 1.0.
//	As it stands today the GL_OES_EGL_image_external extension does not currently specify interactions with ESSL3. We are working with Khronos to amend the extension. In the meantime, you have the right workaround - we recommend using ESSL1 for this use case for the time being.
//	http://community.arm.com/thread/5204

//	for glsl3 support, try this extension
//	https://www.khronos.org/registry/gles/extensions/OES/OES_EGL_image_external_essl3.txt
static auto BlitFragShaderOesExternal __unused =
#include "BlitOesExternal.glsl.frag"
;

static auto BlitFragShaderTest =
#include "BlitTest.glsl.frag"
;

static auto BlitFragShaderError =
#include "BlitError.glsl.frag"
;

//	gr: have this conversion somewhere in the lib
static auto BlitFragShaderFreenectDepth10mm =
#include "BlitFreenectDepth10mm.glsl.frag"
;



Renderer::TBlitter::TBlitter(std::shared_ptr<TPool<TTexture>> TexturePool) :
	mTempTextures	( TexturePool )
{
	if ( !mTempTextures )
		mTempTextures.reset( new TPool<TTexture> );
}

Renderer::TBlitter::~TBlitter()
{
	mBlitShaders.clear();
	mBlitQuad.reset();
	mRenderTarget.reset();
}

TPool<Renderer::TTexture>& Renderer::TBlitter::GetTexturePool()
{
	return *mTempTextures;
}

TPool<SoyPixelsImpl>& Renderer::TBlitter::GetPixelPool()
{
	if ( !mTempPixels )
		mTempPixels.reset( new TPool<SoyPixelsImpl> );
	
	return *mTempPixels;
}


std::shared_ptr<Renderer::TShader> Renderer::TBlitter::GetShader(const std::string& Name,const char* Source,Renderer::TContext& Context)
{
	return nullptr;
	/*
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
		auto pShader = std::make_shared<Renderer::TShader>( BlitVertShader, Source, VertexDesc, Name, Context );
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

		std::Debug << "Failed to allocate shader " << Name << ", reverting to test shader... Exception: " << e.what()  << std::endl;
		return GetBackupShader( Context );
	}
	 */
}



std::shared_ptr<Renderer::TShader> Renderer::TBlitter::GetBackupShader(TContext& Context)
{
	return GetShader("Backup", BlitFragShaderTest, Context );
}

std::shared_ptr<Renderer::TShader> Renderer::TBlitter::GetErrorShader(TContext& Context)
{
	return GetShader("Error", BlitFragShaderError, Context );
}


std::shared_ptr<Renderer::TShader> Renderer::TBlitter::GetShader(ArrayBridge<Renderer::TTexture>& Sources,Renderer::TContext& Context)
{
	if ( Sources.GetSize() == 0 )
		return GetBackupShader(Context);
	
	auto& Texture0 = Sources[0];
	
	//	alloc and return the shader we need
	if ( Sources.GetSize() > 1 && mMergeYuv )
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
			
			//case SoyPixelsFormat::Yuv_844_Full:		return GetShader( SoyPixelsFormat::ToString(MergedFormat), BlitFragShader_Yuv_844_Full, Context );
			//case SoyPixelsFormat::Yuv_844_Ntsc:		return GetShader( SoyPixelsFormat::ToString(MergedFormat), BlitFragShader_Yuv_844_Ntsc, Context );
			//case SoyPixelsFormat::Yuv_844_Smptec:	return GetShader( SoyPixelsFormat::ToString(MergedFormat), BlitFragShader_Yuv_844_Smptec, Context );
			
			default:break;
		}

		std::Debug << "Warning: No frag shader specified for merged format " << MergedFormat << " (" << Texture0.GetMeta().GetFormat() << " + " << Texture1.GetMeta().GetFormat() << ")" << std::endl;
	}

	if ( Texture0.GetMeta().GetFormat() == SoyPixelsFormat::FreenectDepthmm )
		return GetShader( SoyPixelsFormat::ToString(SoyPixelsFormat::FreenectDepthmm), BlitFragShaderFreenectDepth10mm, Context );

	return GetShader( "2D", BlitFragShader2D, Context  );
}




std::shared_ptr<Renderer::TRenderTarget> Renderer::TBlitter::GetRenderTarget(Renderer::TTexture& Target,Renderer::TContext& Context)
{
	/*
	if ( mRenderTarget )
	{
		if ( mRenderTarget->mTexture.mTexture.mName == Target.mTexture.mName )
			//	if ( mRenderTarget->mMeta == TargetTexture.mMeta )
			return mRenderTarget;
		mRenderTarget.reset();
	}
	
	Renderer::TFboMeta FboMeta(__func__, Target.GetWidth(), Target.GetHeight() );
	//	create immediately
	mRenderTarget.reset( new Renderer::TRenderTargetFbo(FboMeta,Target) );

	//	turn off auto mip map generation and do it manually later
	auto& TargetFbo = dynamic_cast<Renderer::TRenderTargetFbo&>( *mRenderTarget );
	TargetFbo.mGenerateMipMaps = false;
	
	//	clear for debugging
	mRenderTarget->Bind();
	Renderer::ClearColour( PopMovie::AllocClearColour );
	mRenderTarget->Unbind();

	//	option to recreate every use
	static bool Store = true;

	//	gr: windows is corrupting(INVALID_OPERATION on 2nd bind) my render target. if I recreate it, problem goes away???
#if defined(TARGET_WINDOWS)
	Store = false;
#endif

	if ( !Store )
	{
		std::shared_ptr<Renderer::TRenderTarget> Target = mRenderTarget;
		mRenderTarget.reset();
		return Target;
	}
	 */
	return mRenderTarget;
}



std::shared_ptr<Renderer::TGeometry> Renderer::TBlitter::GetGeo(Renderer::TContext& Context)
{
	return nullptr;
	/*
	if ( mBlitQuad )
		return mBlitQuad;
	
	//	for each part of the vertex, add an attribute to describe the overall vertex
	Opengl::TGeometryVertex Vertex;
	Array<uint8> MeshData;
	Array<size_t> Indexes;
	Soy::TBlitter::GetGeo( Vertex, GetArrayBridge(MeshData), GetArrayBridge(Indexes), false );

	mBlitQuad.reset( new Renderer::TGeometry( GetArrayBridge(MeshData), GetArrayBridge(Indexes), Vertex ) );
	return mBlitQuad;
	 */
}

void Renderer::TBlitter::BlitError(Renderer::TTexture& Target,const std::string& Error,Renderer::TContext& Context)
{
	//	gr: until we have text blitting, we need to at least print out the error
	std::Debug << "BlitError(" << Error << ")" << std::endl;

	//	gr: add non-
	std::shared_ptr<TShader> ErrorShader = GetErrorShader( Context );

	BufferArray<TTexture,2> Textures;
	SoyGraphics::TTextureUploadParams UploadParams;
	BlitTexture( Target, GetArrayBridge(Textures), Context, UploadParams, ErrorShader );
}


void Renderer::TBlitter::BlitTexture(Renderer::TTexture& Target,ArrayBridge<Renderer::TTexture>&& Sources,Renderer::TContext& Context,const SoyGraphics::TTextureUploadParams& UploadParams,const char* OverrideShader)
{
	//	grab provided shader if it's already compiled
	std::shared_ptr<TShader> OverrideShaderPtr;
	if ( OverrideShader )
	{
		OverrideShaderPtr = GetShader( "OverrideShader", OverrideShader, Context );
	}
	
	BlitTexture( Target, GetArrayBridge(Sources), Context, UploadParams, OverrideShaderPtr );
}

void Renderer::TBlitter::BlitTexture(Renderer::TTexture& Target,ArrayBridge<Renderer::TTexture>&& Sources,Renderer::TContext& Context,const SoyGraphics::TTextureUploadParams& UploadParams,std::shared_ptr<Renderer::TShader> OverrideShader)
{
	ofScopeTimerWarning Timer("Blit Renderer Texture[s]",4);
	
	//	first implementation, just copy
	if ( Sources.IsEmpty() )
		throw Soy::AssertException("expected some textures for Blit");
	
	auto& Texture0 = Sources[0];
	Target.Write( Texture0, UploadParams, Context );
	
	/*
	//	gr: pre-empt this!
	//		or use blit error?
	if ( !Context.IsSupported( OpenglExtensions::VertexArrayObjects ) )
		throw Soy::AssertException("VAO not supported");
	
	if ( mUseTestBlit && !OverrideShader )
		OverrideShader = GetBackupShader( Context );
	
	
	//	gr: pre-empt these failures too
	auto RenderTarget = GetRenderTarget( Target, Context );
	auto BlitGeo = GetGeo( Context );
	auto BlitShader = OverrideShader ? OverrideShader : GetShader( Sources, Context );
	
	Soy::Assert( RenderTarget!=nullptr, "Failed to get render target for blit");
	Soy::Assert( BlitGeo!=nullptr, "Failed to get geo for blit");
	Soy::Assert( BlitShader!=nullptr,"Failed to get shader for blit");
	
	//	do bindings
	RenderTarget->Bind();

	//	for debugging
#if defined(TARGET_WINDOWS)
	static bool SkipRender = false;
	if (SkipRender)
	{
		RenderTarget->Unbind();
		return;
	}
#endif

	//	clear FBO
	static bool ClearDepth = false;
	static bool ClearColour = false;
	static bool ClearStencil = false;
	if ( ClearDepth )	Renderer::ClearDepth();
	if ( ClearColour )	Renderer::ClearColour( PopMovie::BlitClearColour );
	if ( ClearStencil )	Renderer::ClearStencil();

	{
		auto Shader = BlitShader->Bind();

		//	gr: if we throw, then there's a hardware error setting a uniform. Don't render as we may be in a bad state
		try
		{
			static bool setuniforms = true;
			if (setuniforms)
			{
				Shader.SetUniform("Transform", mTransform );
				//	try to set all the uniforms
				//	assume pixel size might fail, not all shaders need it
				const char* TextureUniforms[] = { "Texture0", "Texture1" };
				const char* TextureSizeUniforms[] = { "Texture0_PixelSize", "Texture1_PixelSize" };
				for (int t = 0; t < Sources.GetSize(); t++)
				{
					Shader.SetUniform(TextureUniforms[t], Sources[t]);
					vec2f PixelSize(Sources[t].GetWidth(), Sources[t].GetHeight());
					Shader.SetUniform(TextureSizeUniforms[t], PixelSize);
				}
			}

			static bool Draw = true;
			if (Draw)
			{
				BlitGeo->Draw();
			}

			static bool Finish = false;
			if (Finish)
				glFinish();
			static bool Flush = false;
			if (Flush)
				glFlush();
		}
		catch (std::exception& e)
		{
			std::Debug << "TBlitter::BlitTexture error: " << e.what() << std::endl;
		}
	}

	//	flush or sync here to ensure texture is used, and now availible to free
	RenderTarget->Unbind();
	
	//	generate mip maps if desired (off by default)
	if ( UploadParams.mGenerateMipMaps )
	{
		Target.GenerateMipMaps();
	}
	 */
}

void Renderer::TBlitter::BlitTexture(Renderer::TTexture& Target,ArrayBridge<const SoyPixelsImpl*>&& OrigSources,Renderer::TContext& Context,const SoyGraphics::TTextureUploadParams& UploadParams,const char* OverrideShader)
{
	ofScopeTimerWarning Timer("Blit SoyPixel Texture[s]",2);
	
	//	expand sub textures
	Array<std::shared_ptr<SoyPixelsImpl>> SplitSources;
	BufferArray<SoyPixelsImpl*,10> Sources;
	for ( int i=0;	i<OrigSources.GetSize();	i++ )
	{
		if ( !OrigSources[i] )
		{
			std::Debug << "Unexpected null pixels " << i << "/" << OrigSources.GetSize() << " in blit" << std::endl;
			continue;
		}
		//	gr: non const for SplitPlanes... not neccesarily unsafe, but then means we need a const std::shared<SoyPixelsImpl>...
		auto& Source = const_cast<SoyPixelsImpl&>( *OrigSources[i] );

		//	if the format has multiple planes we need to split the image
		Array<std::shared_ptr<SoyPixelsImpl>> PixelPlanes;

		Source.SplitPlanes( GetArrayBridge(PixelPlanes) );
		for ( int p=0;	p<PixelPlanes.GetSize();	p++ )
		{
			SplitSources.PushBack( PixelPlanes[p] );
			Sources.PushBack( PixelPlanes[p].get() );
		}
	}
	
	if ( Sources.IsEmpty() )
	{
		throw Soy::AssertException( std::string(__func__) + " no sources" );
	}
	
	//	simple write when there's just one pixel buferr
	//	need this function in case we need to force a blit for single-plane pixel buffer uploads, which we could normally skip
	//	gr: if we DONT blit, we won't get the opengl upside down flip, so we DONT look for identity here, we look for upside down
	//	gr: DX needs something similar
	static bool RealForceBlit = false;
	static float3x3 UpsideDownTransform = float3x3( -1, 0, 0,
												   0, -1, 0,
												   0, 0, 1 );
	bool ForceBlit = RealForceBlit || HasWatermark() || (mTransform != UpsideDownTransform);

	if ( Sources.GetSize() == 1 && !ForceBlit )
	{
		Target.Write( *Sources[0], UploadParams, Context );
		return;
	}
	
	//	copy each to it's own texture, then do a blit
	BufferArray<Renderer::TTexture,4> SourceTextures;
	BufferArray<std::shared_ptr<SoyPixelsImpl>,4> PaddedSourcePixels;
	
	auto DeallocTempTextures = [this,&SourceTextures,&PaddedSourcePixels]
	{
		auto& TexturePool = GetTexturePool();
		for ( int t=0;	t<SourceTextures.GetSize();	t++ )
			TexturePool.Release( SourceTextures[t] );
		
		auto& PixelPool = GetPixelPool();
		for ( int t=0;	t<PaddedSourcePixels.GetSize();	t++ )
			PixelPool.Release( PaddedSourcePixels[t] );
	};
	
	try
	{
		for ( int i=0;	i<Sources.GetSize();	i++ )
		{
			auto& UnpaddedSource = *Sources[i];
			std::shared_ptr<SoyPixelsImpl> PaddedSource;
			
			//	metal may need to pad pixels
			SoyPixelsMeta PaddedMeta = Metal::TTexture::CorrectPixelsForUpload( UnpaddedSource.GetMeta() );
			if ( PaddedMeta != UnpaddedSource.GetMeta() )
			{
				std::stringstream TimerName;
				TimerName << "Warning: padding pixels for meta (" << UnpaddedSource.GetMeta() << " -> " << PaddedMeta << ")";
				ofScopeTimerWarning PadTimer( TimerName.str().c_str(), 2 );

				auto& PixelPool = GetPixelPool();
				auto AllocPaddedPixels = [&PaddedMeta]
				{
					return std::shared_ptr<SoyPixelsImpl>( new SoyPixels(PaddedMeta) );
				};
				PaddedSource = PixelPool.AllocPtr( PaddedMeta, AllocPaddedPixels );
				PaddedSourcePixels.PushBack( PaddedSource );
				PaddedSource->Copy( UnpaddedSource, TSoyPixelsCopyParams(true,true,true,false,false) );
			}
			
			//	allocate a dynamic texture to put pixels into
			auto& Source = PaddedSource ? *PaddedSource : UnpaddedSource;
			auto& PixelTexture = GetTempTexture( Source.GetMeta(), Context );
			SourceTextures.PushBack( PixelTexture );

			PixelTexture.Write( Source, UploadParams, Context );
		}
		
		//	do normal blit
		BlitTexture( Target, GetArrayBridge(SourceTextures), Context, UploadParams, OverrideShader );
		DeallocTempTextures();
	}
	catch(...)
	{
		DeallocTempTextures();
		throw;
	}
}



std::shared_ptr<Renderer::TTexture> Renderer::TBlitter::GetTempTexturePtr(SoyPixelsMeta Meta,TContext& Context)
{
	auto RealAlloc = [&Meta,&Context]()
	{
		return std::shared_ptr<TTexture>( new TTexture(Meta,Context) );
	};
	
	auto& Pool = GetTexturePool();
	auto Texture = Pool.AllocPtr( Meta, RealAlloc );
	return Texture;
}


Renderer::TTexture& Renderer::TBlitter::GetTempTexture(SoyPixelsMeta Meta,TContext& Context)
{
	auto pTexture = GetTempTexturePtr( Meta, Context );
	Soy::Assert( pTexture!=nullptr, "Failed to alloc temp texture");
	return *pTexture;
}


void Renderer::TBlitter::GetMeta(TJsonWriter& Json)
{
	Soy::TBlitter::GetMeta( Json );

	auto& Pool = GetTexturePool();

	Json.Push("BlitterTexturePoolSize", Pool.GetAllocCount() );
}

