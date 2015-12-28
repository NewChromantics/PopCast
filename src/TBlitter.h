#pragma once


#include "TBlitterWatermark.h"



#include <SoyVector.h>

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
		mUseTestBlit	( false )
	{
	}

	//	gr: function in case we can cache it in future and skip the SetUniform later
	void			SetTransform(const float3x3& Transform)		{	mTransform = Transform;	}
	void			SetUseTestBlitShader(bool UseTestBlit=true)	{	mUseTestBlit = UseTestBlit;	}
	
	static void		GetGeo(Opengl::TGeometryVertex& Description,ArrayBridge<uint8>&& VertexData,ArrayBridge<size_t>&& Indexes,bool DirectxMode);
	static bool		HasWatermark();			//	need this function in case we need to force a blit for single-plane pixel buffer uploads, which we could normally skip

public:
	bool			mUseTestBlit;			//	force using the test blit shader
	float3x3		mTransform;				//	affine transform for source -> dest
};


