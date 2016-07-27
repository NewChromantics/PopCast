#pragma once

#include <SoyMetal.h>
#include "TBlitter.h"
#include <SoyPool.h>


namespace Metal
{
	class TBlitter;
}



class Metal::TBlitter : public Soy::TBlitter
{
public:
	TBlitter(std::shared_ptr<TPool<TTexture>> TexturePool);
	~TBlitter();

	virtual void	GetMeta(TJsonWriter& Json) override;

	void			BlitTexture(TTexture& Target,ArrayBridge<TTexture>&& Source,TContext& Context,const SoyGraphics::TTextureUploadParams& UploadParams,std::shared_ptr<TShader> OverrideShader=nullptr);
	void			BlitTexture(TTexture& Target,ArrayBridge<TTexture>&& Source,TContext& Context,const SoyGraphics::TTextureUploadParams& UploadParams,const char* OverrideShader);
	void			BlitTexture(TTexture& Target,ArrayBridge<const SoyPixelsImpl*>&& Source,TContext& Context,const SoyGraphics::TTextureUploadParams& UploadParams,const char* OverrideShader=nullptr);
	void			BlitError(TTexture& Target,const std::string& Error,TContext& Context);
	
	std::shared_ptr<TRenderTarget>			GetRenderTarget(TTexture& Target,TContext& Context);
	std::shared_ptr<TGeometry>				GetGeo(TContext& Context);
	std::shared_ptr<TShader>				GetShader(ArrayBridge<TTexture>& Sources,TContext& Context);

	std::shared_ptr<TShader>				GetShader(const std::string& Name,const char* Source,TContext& Context);
	std::shared_ptr<TShader>				GetBackupShader(TContext& Context);		//	shader for when a shader doesn't compile
	std::shared_ptr<TShader>				GetErrorShader(TContext& Context);

	std::shared_ptr<TTexture>				GetTempTexturePtr(SoyPixelsMeta Meta,TContext& Context);
	TTexture&								GetTempTexture(SoyPixelsMeta Meta,TContext& Context);

protected:
	TPool<TTexture>&						GetTexturePool();
	TPool<SoyPixelsImpl>&					GetPixelPool();

public:
	std::shared_ptr<TPool<TTexture>>		mTempTextures;
	std::shared_ptr<TPool<SoyPixelsImpl>>	mTempPixels;
	std::shared_ptr<TRenderTarget>			mRenderTarget;
	std::shared_ptr<TGeometry>				mBlitQuad;
	std::map<const char*,std::shared_ptr<TShader>>	mBlitShaders;	//	shader mapped to it's literal content address
};


