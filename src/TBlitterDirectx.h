#pragma once

#include <SoyDirectx.h>
#include "TBlitter.h"


namespace Directx
{
	class TBlitter;
}


class Directx::TBlitter : public Soy::TBlitter
{
public:
	void						BlitTexture(TTexture& Target,ArrayBridge<SoyPixelsImpl*>&& Source,TContext& Context,std::shared_ptr<TShader> OverrideShader=nullptr);
	void						BlitError(TTexture& Target,const std::string& Error,TContext& Context);
	
	TTexture&					GetTempTexture(SoyPixelsMeta Meta,TContext& Context,TTextureMode::Type Mode);			//	alloc/find a temp texture from the pool. throw if we can't make one
	std::shared_ptr<TTexture>	GetTempTexturePtr(SoyPixelsMeta Meta,TContext& Context,TTextureMode::Type Mode);			//	alloc/find a temp texture from the pool. throw if we can't make one
	TRenderTarget&				GetRenderTarget(std::shared_ptr<TTexture>& Texture,TContext& Context);
	
	std::shared_ptr<Directx::TGeometry>		GetGeo(Directx::TContext& Context);
	std::shared_ptr<Directx::TShader>		GetTestShader(Directx::TContext& Context);
	std::shared_ptr<Directx::TShader>		GetErrorShader(Directx::TContext& Context);
	std::shared_ptr<Directx::TShader>		GetShader(ArrayBridge<SoyPixelsImpl*>& Sources,Directx::TContext& Context);

public:
	//	pools
	Array<std::shared_ptr<TTexture>>		mTempTextures;	
	Array<std::shared_ptr<TRenderTarget>>	mRenderTargets;	
	std::shared_ptr<Directx::TGeometry>		mBlitQuad;
	std::shared_ptr<Directx::TShader>		mBlitShaderTest;
	std::shared_ptr<Directx::TShader>		mBlitShaderError;
	std::shared_ptr<Directx::TShader>		mBlitShaderRgba;
	std::shared_ptr<Directx::TShader>		mBlitShaderYuvVideo;
	std::shared_ptr<Directx::TShader>		mBlitShaderYuvFull;
};


