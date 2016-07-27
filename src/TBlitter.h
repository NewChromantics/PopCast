#pragma once

#include <SoyVector.h>
#include <SoyGraphics.h>


#define WATERMARK_PREAMBLE_GLSL	""
#define WATERMARK_PREAMBLE_HLSL	""


class TJsonWriter;

namespace Soy
{
	class TBlitter;
}

namespace Opengl
{
	extern const char*	BlitVertShader;
};

namespace Directx
{
	extern const char*	BlitVertShader;
};


class Soy::TBlitter
{
public:
	TBlitter() :
		mUseTestBlit			( false ),
		mBlitMergeYuv			( true ),
		mClearBeforeBlit		( true ),
		mClearBeforeBlitColour	( 0,1,1 )
	{
	}
	
	virtual void	GetMeta(TJsonWriter& Json)	{}	//	todo

	//	gr: function in case we can cache it in future and skip the SetUniform later
	void			SetTransform(const float3x3& Transform)		{	mTransform = Transform;	}
	void			SetUseTestBlitShader(bool UseTestBlit=true)	{	mUseTestBlit = UseTestBlit;	}
	void			SetMergeYuv(bool MergeYuv=true)				{	mBlitMergeYuv = MergeYuv;	}
	void			SetClearBeforeBlit(bool ClearBeforeBlit=true)	{	mClearBeforeBlit = ClearBeforeBlit;	}
	
	static void		GetGeo(SoyGraphics::TGeometryVertex& Description,ArrayBridge<uint8>&& VertexData,ArrayBridge<size_t>&& Indexes,bool DirectxMode);
	static void		GetGeoWithPositions(SoyGraphics::TGeometryVertex& Description,ArrayBridge<uint8>&& VertexData,ArrayBridge<size_t>&& Indexes);
	static bool		HasWatermark();			//	need this function in case we need to force a blit for single-plane pixel buffer uploads, which we could normally skip

public:
	bool			mBlitMergeYuv;			//	can force merging of YUV off
	bool			mUseTestBlit;			//	force using the test blit shader
	float3x3		mTransform;				//	affine transform for source -> dest

	bool			mClearBeforeBlit;
	Soy::TRgb		mClearBeforeBlitColour;
};


