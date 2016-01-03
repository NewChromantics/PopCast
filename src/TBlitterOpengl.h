#pragma once

#include <SoyOpenglContext.h>
#include "TBlitter.h"

namespace Opengl
{
	class TBlitter;
}



class Opengl::TBlitter : public Soy::TBlitter
{
public:
	~TBlitter();
	void			BlitTexture(TTexture& Target,ArrayBridge<Opengl::TTexture>&& Source,TContext& Context,const TTextureUploadParams& UploadParams,std::shared_ptr<Opengl::TShader> OverrideShader=nullptr);
	void			BlitTexture(TTexture& Target,ArrayBridge<Opengl::TTexture>&& Source,TContext& Context,const TTextureUploadParams& UploadParams,const char* OverrideShader);
	void			BlitTexture(TTexture& Target,ArrayBridge<SoyPixelsImpl*>&& Source,TContext& Context,const TTextureUploadParams& UploadParams,const char* OverrideShader=nullptr);
	void			BlitError(TTexture& Target,const std::string& Error,TContext& Context);
	
	std::shared_ptr<Opengl::TRenderTarget>	GetRenderTarget(Opengl::TTexture& Target,Opengl::TContext& Context);
	std::shared_ptr<Opengl::TGeometry>		GetGeo(Opengl::TContext& Context);
	std::shared_ptr<Opengl::TShader>		GetShader(ArrayBridge<Opengl::TTexture>& Sources,Opengl::TContext& Context);
	std::shared_ptr<Opengl::TShader>		GetTestShader(Opengl::TContext& Context);
	std::shared_ptr<Opengl::TShader>		GetErrorShader(Opengl::TContext& Context);

	std::shared_ptr<Opengl::TTexture>		GetTempTexturePtr(SoyPixelsMeta Meta,TContext& Context,GLenum Mode);
	Opengl::TTexture&						GetTempTexture(SoyPixelsMeta Meta,TContext& Context,GLenum Mode);

public:
	Array<std::shared_ptr<Opengl::TTexture>>	mTempTextures;			//	temp textures used for YUV pixel blits to textures
	std::map<const char*,std::shared_ptr<Opengl::TShader>>	mBlitShaders;	//	shader mapped to it's literal content address

	std::shared_ptr<Opengl::TShader>			mBlitShader2D;			//	for GL_TEXTURE_2D
	std::shared_ptr<Opengl::TShader>			mBlitShaderTest;		//	no textures
	std::shared_ptr<Opengl::TShader>			mBlitShaderError;		//	no textures
	std::shared_ptr<Opengl::TShader>			mBlitShaderRect;		//	for GL_TEXTURE_RECTANGLE
	std::shared_ptr<Opengl::TShader>			mBlitShaderOes;			//	for GL_TEXTURE_EXTERNAL
	std::shared_ptr<Opengl::TShader>			mBlitShaderYuvFull;		//	luma+chroma = rgb
	std::shared_ptr<Opengl::TShader>			mBlitShaderYuvVideo;	//	luma+chroma = rgb
	std::shared_ptr<Opengl::TShader>			mBlitShaderFreenectDepth10mm;	//	extract bad depths & convert to linear
	std::shared_ptr<Opengl::TGeometry>			mBlitQuad;
	std::shared_ptr<Opengl::TRenderTargetFbo>	mRenderTarget;
};


