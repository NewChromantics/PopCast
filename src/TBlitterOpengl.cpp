#include "TBlitterOpengl.h"
#include "TBlitter.h"
#include <SoyMedia.h>
#include <SoyJson.h>



namespace PopMovie
{
	Soy::TRgb	AllocClearColour = Soy::TRgb(1,0,1);
	Soy::TRgb	BlitClearColour = Soy::TRgb(1,1,0);
}

//	gl es 2 requires precision specification. todo; add this to the shader upgrader if it's the first thing on the line
//	or a varying

//	ES2 (what glsl?) requires precision, but not on mat?
//	ES3 glsl 100 (android s6) requires precision on vec's and mat
//	gr: changed to highprecision to fix sampling aliasing with the bjork video
//	gr: pretty much unused now (easier to put highp in the code), but left for for the comments
#define PREC	"highp "



const char* Opengl::BlitVertShader =
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



Opengl::TBlitter::TBlitter(std::shared_ptr<TPool<TTexture>> TexturePool) :
	mTempTextures	( TexturePool )
{
	if ( !mTempTextures )
		mTempTextures.reset( new TPool<TTexture> );
}

Opengl::TBlitter::~TBlitter()
{
	mBlitShaders.clear();
	mBlitQuad.reset();
	mRenderTarget.reset();
}

TPool<Opengl::TTexture>& Opengl::TBlitter::GetTexturePool()
{
	return *mTempTextures;
}


std::shared_ptr<Opengl::TShader> Opengl::TBlitter::GetShader(const std::string& Name,const char* Source,Opengl::TContext& Context)
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
		auto pShader = std::make_shared<Opengl::TShader>( BlitVertShader, Source, VertexDesc, Name, Context );
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
}



std::shared_ptr<Opengl::TShader> Opengl::TBlitter::GetBackupShader(TContext& Context)
{
	return GetShader("Backup", BlitFragShaderTest, Context );
}

std::shared_ptr<Opengl::TShader> Opengl::TBlitter::GetErrorShader(TContext& Context)
{
	return GetShader("Error", BlitFragShaderError, Context );
}


std::shared_ptr<Opengl::TShader> Opengl::TBlitter::GetShader(ArrayBridge<Opengl::TTexture>& Sources,Opengl::TContext& Context)
{
	if ( Sources.GetSize() == 0 )
		return GetBackupShader(Context);
	
	auto& Texture0 = Sources[0];
	
	//	alloc and return the shader we need
	if ( Sources.GetSize() > 1 && mMergeYuv )
	{
		auto& Texture1 = Sources[1];
		auto MergedFormat = SoyPixelsFormat::GetMergedFormat( Texture0.mMeta.GetFormat(), Texture1.mMeta.GetFormat() );

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

		std::Debug << "Warning: No frag shader specified for merged format " << MergedFormat << " (" << Texture0.mMeta.GetFormat() << " + " << Texture1.mMeta.GetFormat() << ")" << std::endl;
	}

	if ( Texture0.mMeta.GetFormat() == SoyPixelsFormat::FreenectDepthmm )
		return GetShader( SoyPixelsFormat::ToString(SoyPixelsFormat::FreenectDepthmm), BlitFragShaderFreenectDepth10mm, Context );
	
	//	gr: note; these are all for rgba, don't we ever need to merge?
	if ( Texture0.mType == GL_TEXTURE_2D )
		return GetShader( "2D", BlitFragShader2D, Context  );
	
#if defined(GL_TEXTURE_RECTANGLE)
	if ( Texture0.mType == GL_TEXTURE_RECTANGLE )
		return GetShader( "Rectangle", BlitFragShaderRectangle, Context  );
#endif
	
#if defined(GL_TEXTURE_EXTERNAL_OES)
	if ( Texture0.mType == GL_TEXTURE_EXTERNAL_OES )
		return GetShader( "OesExternal", BlitFragShaderOesExternal, Context );
#endif
	
	//	don't know which shader to use!
	std::Debug << "Don't know what blit shader to use for texture " << Opengl::GetEnumString(Texture0.mType) << '/' << Texture0.mMeta << std::endl;
	return GetBackupShader(Context);
}




std::shared_ptr<Opengl::TRenderTarget> Opengl::TBlitter::GetRenderTarget(Opengl::TTexture& Target,Opengl::TContext& Context)
{
	if ( mRenderTarget )
	{
		if ( mRenderTarget->mTexture.mTexture.mName == Target.mTexture.mName )
			//	if ( mRenderTarget->mMeta == TargetTexture.mMeta )
			return mRenderTarget;
		mRenderTarget.reset();
	}
	
	Opengl::TFboMeta FboMeta(__func__, Target.GetWidth(), Target.GetHeight() );
	//	create immediately
	mRenderTarget.reset( new Opengl::TRenderTargetFbo(FboMeta,Target) );

	//	turn off auto mip map generation and do it manually later
	auto& TargetFbo = dynamic_cast<Opengl::TRenderTargetFbo&>( *mRenderTarget );
	TargetFbo.mGenerateMipMaps = false;
	
	//	clear for debugging
	mRenderTarget->Bind();
	Opengl::ClearColour( PopMovie::AllocClearColour );
	mRenderTarget->Unbind();

	//	option to recreate every use
	static bool Store = true;

	//	gr: windows is corrupting(INVALID_OPERATION on 2nd bind) my render target. if I recreate it, problem goes away???
#if defined(TARGET_WINDOWS)
	Store = false;
#endif

	if ( !Store )
	{
		std::shared_ptr<Opengl::TRenderTarget> Target = mRenderTarget;
		mRenderTarget.reset();
		return Target;
	}
	return mRenderTarget;
}



std::shared_ptr<Opengl::TGeometry> Opengl::TBlitter::GetGeo(Opengl::TContext& Context)
{
	if ( mBlitQuad )
		return mBlitQuad;
	
	//	for each part of the vertex, add an attribute to describe the overall vertex
	Opengl::TGeometryVertex Vertex;
	Array<uint8> MeshData;
	Array<size_t> Indexes;
	Soy::TBlitter::GetGeo( Vertex, GetArrayBridge(MeshData), GetArrayBridge(Indexes), false );

	mBlitQuad.reset( new Opengl::TGeometry( GetArrayBridge(MeshData), GetArrayBridge(Indexes), Vertex ) );
	return mBlitQuad;
}

void Opengl::TBlitter::BlitError(Opengl::TTexture& Target,const std::string& Error,Opengl::TContext& Context)
{
	//	gr: until we have text blitting, we need to at least print out the error
	std::Debug << "BlitError(" << Error << ")" << std::endl;

	//	gr: add non-
	std::shared_ptr<TShader> ErrorShader = GetErrorShader( Context );

	BufferArray<TTexture,2> Textures;
	TTextureUploadParams UploadParams;
	BlitTexture( Target, GetArrayBridge(Textures), Context, UploadParams, ErrorShader );
}


void Opengl::TBlitter::BlitTexture(Opengl::TTexture& Target,ArrayBridge<Opengl::TTexture>&& Sources,Opengl::TContext& Context,const TTextureUploadParams& UploadParams,const char* OverrideShader)
{
	//	grab provided shader if it's already compiled
	std::shared_ptr<TShader> OverrideShaderPtr;
	if ( OverrideShader )
	{
		OverrideShaderPtr = GetShader( "OverrideShader", OverrideShader, Context );
	}
	
	BlitTexture( Target, GetArrayBridge(Sources), Context, UploadParams, OverrideShaderPtr );
}

void Opengl::TBlitter::BlitTexture(Opengl::TTexture& Target,ArrayBridge<Opengl::TTexture>&& Sources,Opengl::TContext& Context,const TTextureUploadParams& UploadParams,std::shared_ptr<Opengl::TShader> OverrideShader)
{
	ofScopeTimerWarning Timer("Blit opengl Texture[s]",4);
	
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

	//	clear FBO
	static bool ClearDepth = false;
	static bool ClearColour = false;
	static bool ClearStencil = false;
	if ( ClearDepth )	Opengl::ClearDepth();
	if ( ClearColour )	Opengl::ClearColour( PopMovie::BlitClearColour );
	if ( ClearStencil )	Opengl::ClearStencil();

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
}

void Opengl::TBlitter::BlitTexture(Opengl::TTexture& Target,ArrayBridge<const SoyPixelsImpl*>&& OrigSources,Opengl::TContext& Context,const TTextureUploadParams& UploadParams,const char* OverrideShader)
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
		Target.Write( *Sources[0], UploadParams );
		return;
	}
	
	//	copy each to it's own texture, then do a blit
	BufferArray<Opengl::TTexture,4> SourceTextures;
	for ( int i=0;	i<Sources.GetSize();	i++ )
	{
		auto& Source = *Sources[i];
		
		//	allocate a dynamic texture to put pixels into
		auto& PixelTexture = GetTempTexture( Source.GetMeta(), Context, GL_TEXTURE_2D );

		PixelTexture.Write( Source, UploadParams );
		SourceTextures.PushBack( PixelTexture );
	}
	
	//	do normal blit
	BlitTexture( Target, GetArrayBridge(SourceTextures), Context, UploadParams, OverrideShader );

	auto& Pool = GetTexturePool();
	for ( int t=0;	t<SourceTextures.GetSize();	t++ )
		Pool.Release( SourceTextures[t] );

}



std::shared_ptr<Opengl::TTexture> Opengl::TBlitter::GetTempTexturePtr(SoyPixelsMeta Meta,TContext& Context,GLenum Type)
{
	auto RealAlloc = [&Meta,&Type,&Context]()
	{
		return std::make_shared<TTexture>( Meta, Type );
	};
	
	auto& Pool = GetTexturePool();
	auto Texture = Pool.AllocPtr( TTextureMeta(Meta,Type), RealAlloc );
	return Texture;
}


Opengl::TTexture& Opengl::TBlitter::GetTempTexture(SoyPixelsMeta Meta,TContext& Context,GLenum Mode)
{
	auto pTexture = GetTempTexturePtr( Meta, Context, Mode );
	Soy::Assert( pTexture!=nullptr, "Failed to alloc temp texture");
	return *pTexture;
}


void Opengl::TBlitter::GetMeta(TJsonWriter& Json)
{
	Soy::TBlitter::GetMeta( Json );

	auto& Pool = GetTexturePool();

	Json.Push("BlitterTexturePoolSize", Pool.GetAllocCount() );
}

