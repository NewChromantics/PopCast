#pragma once


#include "TBlitter.h"
#include "SoyGnm.h"
#include "PopMovieDecoder.h"

namespace Gnm
{
	class TBlitter;

	class TRenderTarget;
	class TShader;
	class TVertexShader;
	class TPixelShader;
	class TGeometry;
}



class Gnm::TBlitter : public Soy::TBlitter
{
public:
	TBlitter(const TVideoDecoderParams& Params);
	~TBlitter();

	void			BlitTexture(TTexture& Target,ArrayBridge<TTexture>&& Source,TContext& Context);
	void			BlitTest(TTexture& Target,TContext& Context);
	void			BlitUnityTest(TTexture& Target,TContext& Context);

	TRenderTarget&	GetRenderTarget(TTexture& Target);
	TVertexShader&	GetVertShader(TContext& Context);
	TPixelShader&	GetFragShader(TContext& Context);
	TGeometry&		GetGeometry(TContext& Context);

	std::shared_ptr<TRenderTarget>	mRenderTarget;
	std::shared_ptr<TVertexShader>	mVertShader;
	std::shared_ptr<TPixelShader>	mFragShader;
	std::shared_ptr<TGeometry>		mGeometry;

	TVideoDecoderParams		mParams;
};


