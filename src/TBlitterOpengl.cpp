#include "TBlitterOpengl.h"
#include "TBlitter.h"
#include <SoyMedia.h>


#if (POPMOVIE_WATERMARK!=0)


//	grahams fancy chevron watermark; https://www.shadertoy.com/view/4lSXRW
#define APPLY_WATERMARK_GLSL(rgba,uv)	\
	"float stripexval = mod((1.0-" uv ".y+0.01)*4.0,0.7);\n"	\
	"float stripeyval = mod((" uv ".x+0.01)*10.0,1.2);\n"	\
	"bool stripexa = stripexval  >= mix( 0.0, 0.5, stripeyval+0.0 ) && stripexval <= 1.0-mix( 0.0, 0.5, stripeyval+0.0 ) && stripeyval > 0.7;\n"	\
	"bool stripexb = stripexval  >= mix( 0.0, 0.5, stripeyval+0.2 ) && stripexval <= 1.0-mix( 0.0, 0.5, stripeyval+0.2 );\n"	\
	"bool stripexc = stripexval >= mix( 0.0, 0.5, stripeyval+0.4 ) && stripexval <= 1.0-mix( 0.0, 0.5, stripeyval+0.4 );\n"	\
	"bool stripexd = stripexval >= mix( 0.0, 0.5, stripeyval+0.6 ) && stripexval <= 1.0-mix( 0.0, 0.5, stripeyval+0.6 );\n"	\
	"bool stripex = ( stripexa && !stripexb ) || ( stripexc && !stripexd );\n"	\
	"bool stripey = stripeyval > 0.30;\n"	\
	"if ( stripex && stripey  )\n"	\
	"	" rgba ".rgb = " WATERMARK_RGB_GLSL ";\n"

#else

    #define APPLY_WATERMARK_GLSL(rgba,uv)	"{}"

#endif


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
#define PREC	"highp "


const char* Opengl::BlitVertShader =
//"layout(location = 0) in vec3 TexCoord;\n"
"attribute vec3 TexCoord;\n"
"varying " PREC " vec2 oTexCoord;\n"
"void main()\n"
"{\n"
"   gl_Position = vec4(TexCoord.x,TexCoord.y,0,1);\n"
//	move to view space 0..1 to -1..1
"	gl_Position.xy *= vec2(2,2);\n"
"	gl_Position.xy -= vec2(1,1);\n"
//	flip uv in vertex shader to apply to all
"	oTexCoord = vec2( TexCoord.x, 1.0-TexCoord.y);\n"
"}\n";

static auto BlitFragShader2D =
"varying " PREC " vec2 oTexCoord;\n"
"uniform sampler2D Texture0;\n"
//"const " PREC "mat3 Transform = mat3(	1,0,0,	0,1,0,	0,0,1	);\n"
"uniform " PREC "mat3 Transform;\n"
"void main()\n"
"{\n"
"	" PREC "vec2 uv = (vec3(oTexCoord.x,oTexCoord.y,1)*Transform).xy;\n"
"	" PREC "vec4 rgba = texture2D(Texture0,uv);\n"
APPLY_WATERMARK_GLSL("rgba","uv")

//	on GLES2 on ios, alpha comes out zero. I'm SURE this breaks some platforms (bad compilers?) android maybe. so very specific atm
#if defined(TARGET_IOS)
"	rgba.a = 1.0;\n"
#endif
"	gl_FragColor = rgba;\n"
"}\n";


static std::string BlitFragShaderYuv(const Soy::TYuvParams& Yuv,SoyPixelsFormat::Type ChromaFormat)
{
	std::stringstream Shader;
	Shader <<
	"varying " PREC " vec2 oTexCoord;\n"
	"uniform sampler2D Texture0;\n"
	"uniform sampler2D Texture1;\n"
	//"const " PREC "mat3 Transform = mat3(	1,0,0,	0,1,0,	0,0,1	);\n";
	"uniform " PREC "mat3 Transform;\n";
	
	//	gles compiler won't accept 1 or 0 as a float. force decimal places
	Shader << "const float LumaMin = " << std::fixed << Yuv.mLumaMin << ";\n";
	Shader << "const float LumaMax = " << std::fixed << Yuv.mLumaMax << ";\n";
	Shader << "const float ChromaVRed = " << std::fixed << Yuv.mChromaVRed << ";\n";
	Shader << "const float ChromaUGreen = " << std::fixed << Yuv.mChromaUGreen << ";\n";
	Shader << "const float ChromaVGreen = " << std::fixed << Yuv.mChromaVGreen << ";\n";
	Shader << "const float ChromaUBlue = " << std::fixed << Yuv.mChromaUBlue << ";\n";
	
	Shader <<
	"void main()\n"
	"{\n"
	"	" PREC "vec2 uv = (vec3(oTexCoord.x,oTexCoord.y,1)*Transform).xy;\n"
	//	scale video luma to to 0..1
	"	float Luma = texture2D(Texture0,uv).x;\n"
	"	Luma = (Luma-LumaMin) / (LumaMax-LumaMin);\n"
	;

	//	get chroma
	if ( ChromaFormat == SoyPixelsFormat::ChromaUV_88 )
	{
		Shader << "	" PREC "vec2 ChromaUv = texture2D(Texture1,uv).xy;\n";
	}
	else if ( ChromaFormat == SoyPixelsFormat::ChromaUV_8_8 )
	{
		Shader << "	" PREC "float ChromaU = texture2D(Texture1,uv*vec2(1.0,0.5)+vec2(0,0)).x;\n";
		Shader << "	" PREC "float ChromaV = texture2D(Texture1,uv*vec2(1.0,0.5)+vec2(0,0.5)).x;\n";
		Shader << "	" PREC "vec2 ChromaUv = vec2(ChromaU,ChromaV);\n";
	}
	else
	{
		std::stringstream Error;
		Error << __func__ << " cannot handle chroma format " << ChromaFormat;
		throw Soy::AssertException( Error.str() );
	}
	
	//	convert chroma from 0..1 to -0.5..0.5
	Shader <<
	"	ChromaUv -= vec2(0.5,0.5);\n"
	//	convert to rgb
	"	" PREC "vec4 rgba;\n"
	"	rgba.r = Luma + ChromaVRed * ChromaUv.y;\n"
	"	rgba.g = Luma + ChromaUGreen * ChromaUv.x + ChromaVGreen * ChromaUv.y;\n"
	"	rgba.b = Luma + ChromaUBlue * ChromaUv.x;\n"
	"	rgba.a = 1.0;\n"
	APPLY_WATERMARK_GLSL("rgba","uv")
	"	gl_FragColor = rgba;\n"
	"}\n";
	return Shader.str();
};


auto BlitFragShaderRectangle =
"varying " PREC " vec2 oTexCoord;\n"
"uniform sampler2DRect Texture0;\n"
"uniform " PREC " vec2 Texture0_PixelSize;\n"
//"const " PREC "mat3 Transform = mat3(	1,0,0,	0,1,0,	0,0,1	);\n"
"uniform " PREC "mat3 Transform;\n"
"void main()\n"
"{\n"
"	" PREC " vec2 uv = (vec3(oTexCoord.x,oTexCoord.y,1)*Transform).xy;\n"
"	" PREC " vec4 rgba = texture2DRect(Texture0,uv*Texture0_PixelSize);\n"
	APPLY_WATERMARK_GLSL("rgba","uv")
"	gl_FragColor = rgba;\n"
"}\n";


//	While the OpenGL ES3 API is backwards compatible with the OpenGL ES2 API, and it supports both GLSL 1.0 and 3.0, GLSL 3.0 isn't defined to be backwards compatible with GLSL 1.0.
//	As it stands today the GL_OES_EGL_image_external extension does not currently specify interactions with ESSL3. We are working with Khronos to amend the extension. In the meantime, you have the right workaround - we recommend using ESSL1 for this use case for the time being.
//	http://community.arm.com/thread/5204

//	for glsl3 support, try this extension
//	https://www.khronos.org/registry/gles/extensions/OES/OES_EGL_image_external_essl3.txt
auto BlitFragShaderOesExternal =
"#extension GL_OES_EGL_image_external : require\n"
"varying " PREC " vec2 oTexCoord;\n"
"uniform samplerExternalOES Texture0;\n"
//"const " PREC "mat3 Transform = mat3(	1,0,0,	0,1,0,	0,0,1	);\n"
"uniform " PREC "mat3 Transform;\n"
"void main()\n"
"{\n"
"	" PREC "vec2 uv = (vec3(oTexCoord.x,oTexCoord.y,1)*Transform).xy;\n"
"	" PREC "vec4 rgba = texture2D(Texture0,uv);\n"
//"	rgba.a = 1.0;\n"	//	on jon's sony android, setting alpha turns the scene greeny/purple!? either here or in the frag. NO idea why. direct copy is fine
	APPLY_WATERMARK_GLSL("rgba","uv")
"	gl_FragColor = rgba;\n"
"}\n";

static auto BlitFragShaderTest =
"varying " PREC " vec2 oTexCoord;\n"
"uniform " PREC "mat3 Transform;\n"
"void main()\n"
"{\n"
"	" PREC "vec2 uv = (vec3(oTexCoord.x,oTexCoord.y,1)*Transform).xy;\n"
"	" PREC "vec4 rgba = vec4(uv.x,uv.y,0,1);\n"
"	if ( uv.x < 0.0 )	rgba.xyz=vec3(0,0,1);\n"
"	if ( uv.y < 0.0 )	rgba.xyz=vec3(1,1,0);\n"
APPLY_WATERMARK_GLSL("rgba","uv")
"	gl_FragColor = rgba;\n"
"}\n";


static auto BlitFragShaderError =
"varying " PREC " vec2 oTexCoord;\n"
"uniform " PREC "mat3 Transform;\n"
"void main()\n"
"{\n"
"	" PREC "vec2 uv = (vec3(oTexCoord.x,oTexCoord.y,1)*Transform).xy;\n"
"	" PREC "vec4 rgba = vec4( mix(0.6,1.0,max(uv.x,uv.y)),uv.x*0.3,uv.y*0.3,1);\n"
APPLY_WATERMARK_GLSL("rgba","uv")
"	gl_FragColor = rgba;\n"
"}\n";



//	gr: have this conversion somewhere in the lib
auto BlitFragShaderFreenectDepth10mm =
"varying " PREC " vec2 oTexCoord;\n"
"uniform sampler2D Texture0;\n"
"const float DepthInvalid = 0.f/1000.f;\n"
"const float DepthMax = 10000.f;\n"
"uniform " PREC "mat3 Transform;\n"
"void main()\n"
"{\n"
"	" PREC "vec2 uv = (vec3(oTexCoord.x,oTexCoord.y,1)*Transform).xy;\n"
"	" PREC "vec4 rgba = texture2D(Texture0,uv);\n"
//	get depth as a float
"	" PREC "float Depth = ( (rgba.x * 255.f) + (rgba.y * 255.f * 255.f) ) / DepthMax;"
//	identify bad depth values and write them to the blue channel
"	rgba.z = ( Depth <= DepthInvalid ) ? 1 : 0;\n"
//	convert depth back to a full range float
"	Depth = Depth * ((255.f*255.f)+255.f);\n"
"	rgba.x = mod( Depth, 255.f );\n"
"	rgba.y = (Depth-rgba.x) / (255.f*255.f);\n"
APPLY_WATERMARK_GLSL("rgba","uv")
"	gl_FragColor = rgba;\n"
"}\n";




std::shared_ptr<Opengl::TShader> AllocShader(std::shared_ptr<Opengl::TShader>& Shader,const std::string& Name,const std::string& VertShader,const std::string& FragShader,Opengl::TGeometryVertex& VertexDescription,std::shared_ptr<Opengl::TShader> BackupShader,Opengl::TContext& Context)
{
	if ( Shader )
		return Shader;
	
	try
	{
		Shader.reset( new Opengl::TShader( VertShader, FragShader, VertexDescription, Name, Context ) );
	}
	catch( std::exception& e )
	{
		std::Debug << "Failed to allocate shader: " << e.what() << std::endl;
		return BackupShader;
	}
	
	
	return Shader;
}



std::shared_ptr<Opengl::TShader> Opengl::TBlitter::GetTestShader(Opengl::TContext& Context)
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


std::shared_ptr<Opengl::TShader> Opengl::TBlitter::GetErrorShader(Opengl::TContext& Context)
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


std::shared_ptr<Opengl::TShader> Opengl::TBlitter::GetShader(ArrayBridge<Opengl::TTexture>& Sources,Opengl::TContext& Context)
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
	
	if ( Sources.GetSize() == 0 )
		return mBlitShaderTest;
	
	auto& Texture0 = Sources[0];
	
	//	alloc and return the shader we need
	if ( Sources.GetSize() > 1 )
	{
		auto& Texture1 = Sources[1];
		if ( Texture0.mMeta.GetFormat() == SoyPixelsFormat::LumaVideo )
			return AllocShader( mBlitShaderYuvVideo, SoyPixelsFormat::ToString(Texture0.mMeta.GetFormat()), BlitVertShader, BlitFragShaderYuv( Soy::TYuvParams::Video(), Texture1.GetFormat() ), VertexDesc, mBlitShaderError, Context  );
		
		//if ( Texture0.mMeta.GetFormat() == SoyPixelsFormat::LumaFull )
		return AllocShader( mBlitShaderYuvFull, SoyPixelsFormat::ToString(Texture0.mMeta.GetFormat()), BlitVertShader, BlitFragShaderYuv( Soy::TYuvParams::Full(), Texture1.GetFormat() ), VertexDesc, mBlitShaderError, Context  );
	}
	
	if ( Texture0.mMeta.GetFormat() == SoyPixelsFormat::FreenectDepthmm )
		return AllocShader( mBlitShaderFreenectDepth10mm, SoyPixelsFormat::ToString(SoyPixelsFormat::FreenectDepthmm), BlitVertShader, BlitFragShaderFreenectDepth10mm, VertexDesc, mBlitShaderError, Context  );
	
	//	gr: note; these are all for rgba, don't we ever need to merge?
	if ( Texture0.mType == GL_TEXTURE_2D )
		return AllocShader( mBlitShader2D, "2D", BlitVertShader, BlitFragShader2D, VertexDesc, mBlitShaderError, Context  );
	
#if defined(GL_TEXTURE_RECTANGLE)
	if ( Texture0.mType == GL_TEXTURE_RECTANGLE )
		return AllocShader( mBlitShaderRect, "Rectangle", BlitVertShader, BlitFragShaderRectangle, VertexDesc, mBlitShaderError, Context  );
#endif
	
#if defined(GL_TEXTURE_EXTERNAL_OES)
	if ( Texture0.mType == GL_TEXTURE_EXTERNAL_OES )
		return AllocShader( mBlitShaderOes, "OesExternal", BlitVertShader, BlitFragShaderOesExternal, VertexDesc, mBlitShaderTest, Context );
#endif
	
	//	don't know which shader to use!
	std::Debug << "Don't know what blit shader to use for texture " << Opengl::GetEnumString(Texture0.mType) << std::endl;
	return mBlitShaderTest;
}


Opengl::TBlitter::~TBlitter()
{
	mTempTextures.Clear(true);
	mBlitShader2D.reset();
	mBlitShaderTest.reset();
	mBlitShaderRect.reset();
	mBlitShaderOes.reset();
	mBlitShaderYuvVideo.reset();
	mBlitShaderYuvFull.reset();
	mBlitQuad.reset();
	mRenderTarget.reset();
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
	//	gr: add non-
	std::shared_ptr<Opengl::TShader> ErrorShader = GetErrorShader( Context );

	BufferArray<Opengl::TTexture,2> Textures;
	TTextureUploadParams UploadParams;
	BlitTexture( Target, GetArrayBridge(Textures), Context, UploadParams, ErrorShader );
}


void Opengl::TBlitter::BlitTexture(Opengl::TTexture& Target,ArrayBridge<Opengl::TTexture>&& Sources,Opengl::TContext& Context,const TTextureUploadParams& UploadParams,std::shared_ptr<Opengl::TShader> OverrideShader)
{
	ofScopeTimerWarning Timer("Blit opengl Texture[s]",4);
	
	//	gr: pre-empt this!
	//		or use blit error?
	if ( !Context.IsSupported( OpenglExtensions::VertexArrayObjects ) )
		throw Soy::AssertException("VAO not supported");
	
	if ( mUseTestBlit && !OverrideShader )
		OverrideShader = GetTestShader( Context );
	
	
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

void Opengl::TBlitter::BlitTexture(Opengl::TTexture& Target,ArrayBridge<SoyPixelsImpl*>&& OrigSources,Opengl::TContext& Context,const TTextureUploadParams& UploadParams)
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
		auto& Source = *OrigSources[i];

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
	BlitTexture( Target, GetArrayBridge(SourceTextures), Context, UploadParams );
}



std::shared_ptr<Opengl::TTexture> Opengl::TBlitter::GetTempTexturePtr(SoyPixelsMeta Meta,TContext& Context,GLenum Mode)
{
	//	already have one?
	for ( int t=0;	t<mTempTextures.GetSize();	t++ )
	{
		auto pTexture = mTempTextures[t];
		if ( !pTexture )
			continue;
		
		if ( pTexture->mMeta != Meta )
			continue;
		
		if ( pTexture->mType != Mode )
			continue;
		
		return pTexture;
	}
	
	//	no matching ones, allocate a new one
	std::shared_ptr<TTexture> Texture( new TTexture( Meta, Mode ) );
	mTempTextures.PushBack( Texture );
	return Texture;
}


Opengl::TTexture& Opengl::TBlitter::GetTempTexture(SoyPixelsMeta Meta,TContext& Context,GLenum Mode)
{
	auto pTexture = GetTempTexturePtr( Meta, Context, Mode );
	return *pTexture;
}

