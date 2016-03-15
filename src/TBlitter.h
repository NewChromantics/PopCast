#pragma once

#include <SoyVector.h>


#define WATERMARK_RGB		"1, 0, 0.376"
#define WATERMARK_PREAMBLE_GLSL	"#define WATERMARK_RGB	vec3(" WATERMARK_RGB ")\n"
#define WATERMARK_PREAMBLE_HLSL	"#define WATERMARK_RGB	float3(" WATERMARK_RGB ")\n"



namespace Soy
{
	class TBlitter;
}

namespace Opengl
{
	class TGeometryVertex;
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
		mUseTestBlit	( false ),
		mMergeYuv		( true )
	{
	}

	//	gr: function in case we can cache it in future and skip the SetUniform later
	void			SetTransform(const float3x3& Transform)		{	mTransform = Transform;	}
	void			SetUseTestBlitShader(bool UseTestBlit=true)	{	mUseTestBlit = UseTestBlit;	}
	void			SetMergeYuv(bool MergeYuv=true)				{	mMergeYuv = MergeYuv;	}
	
	static void		GetGeo(Opengl::TGeometryVertex& Description,ArrayBridge<uint8>&& VertexData,ArrayBridge<size_t>&& Indexes,bool DirectxMode);
	static bool		HasWatermark();			//	need this function in case we need to force a blit for single-plane pixel buffer uploads, which we could normally skip

public:
	bool			mMergeYuv;				//	can force merging of YUV off
	bool			mUseTestBlit;			//	force using the test blit shader
	float3x3		mTransform;				//	affine transform for source -> dest
};


