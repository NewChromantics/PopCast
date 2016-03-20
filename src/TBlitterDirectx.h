#pragma once

#include <SoyDirectx.h>
#include "TBlitter.h"
#include <SoyPool.h>

namespace Directx
{
	class TBlitter;
}


class Directx::TBlitter : public Soy::TBlitter
{
public:
	TBlitter(std::shared_ptr<TPool<TTexture>> TexturePool);

	void						BlitTexture(TTexture& Target,ArrayBridge<const SoyPixelsImpl*>&& Source,TContext& Context,std::shared_ptr<TShader> OverrideShader);
	void						BlitTexture(TTexture& Target,ArrayBridge<const SoyPixelsImpl*>&& Source,TContext& Context,const char* OverrideShader=nullptr);
	void						BlitError(TTexture& Target,const std::string& Error,TContext& Context);
	
	TTexture&					GetTempTexture(SoyPixelsMeta Meta,TContext& Context,TTextureMode::Type Mode);			//	alloc/find a temp texture from the pool. throw if we can't make one
	std::shared_ptr<TTexture>	GetTempTexturePtr(SoyPixelsMeta Meta,TContext& Context,TTextureMode::Type Mode);			//	alloc/find a temp texture from the pool. throw if we can't make one
	TRenderTarget&				GetRenderTarget(std::shared_ptr<TTexture>& Texture,TContext& Context);
	
	std::shared_ptr<TGeometry>	GetGeo(Directx::TContext& Context);
	std::shared_ptr<TShader>	GetShader(ArrayBridge<const SoyPixelsImpl*>& Sources,Directx::TContext& Context);

	std::shared_ptr<TShader>	GetShader(const std::string& Name,const char* Source,TContext& Context);
	std::shared_ptr<TShader>	GetBackupShader(TContext& Context);		//	shader for when a shader doesn't compile
	std::shared_ptr<TShader>	GetErrorShader(TContext& Context);

protected:
	TPool<TTexture>&			GetTexturePool();

public:
	//	pools
	std::shared_ptr<TPool<TTexture>>		mTempTextures;	
	Array<std::shared_ptr<TRenderTarget>>	mRenderTargets;	
	std::shared_ptr<Directx::TGeometry>		mBlitQuad;
	std::map<const char*,std::shared_ptr<TShader>>	mBlitShaders;	//	shader mapped to it's literal content address
};


