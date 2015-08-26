#pragma once

#include "SoyDebug.h"
#include "PopUnity.h"


class TCaster;


//	this MUST match the c# struct in alignment and content
struct TPopCastParams
{
#if defined(TARGET_IOS)
	char						mGarbage[12];
#endif
#if defined(TARGET_ANDROID)
	char						mGarbage[8];
#endif
	Unity::ulong				mPreSeekMs = 0;
#if defined(TARGET_OSX)||defined(TARGET_WINDOWS)
	char						mGarbage[4];
#endif
	Unity::Texture2DPixelFormat::Type	mDecodeAsFormat = Unity::Texture2DPixelFormat::RGBA32;
	Unity::boolean				mSkipPushFrames = false;
	Unity::boolean				mSkipPopFrames = true;
	Unity::boolean				mAllowGpuColourConversion = true;
	Unity::boolean				mAllowCpuColourConversion = false;
	Unity::boolean				mAllowSlowCopy = true;
	Unity::boolean				mAllowFastCopy = true;
	Unity::boolean				mPixelClientStorage = false;
	Unity::boolean				mDebugFrameSkipping = true;
	Unity::boolean				mPeekBeforeDefferedCopy = true;
	Unity::boolean				mDecodeAsYuvFull = false;
	Unity::boolean				mDecodeAsYuvVideo = false;
	Unity::boolean				mDebugNoNewPixelBuffer = false;
	Unity::boolean				mDebugRenderThreadCallback = false;
	Unity::boolean				mResetInternalTimestamp = false;
	Unity::boolean				mDebugBlit = false;
	Unity::boolean				mApplyVideoTransform = true;
};

__export Unity::ulong	PopCast_Alloc(const char* Filename,TPopCastParams Params);
__export bool			PopCast_Free(Unity::ulong Instance);
__export void			PopCast_EnumDevices();



namespace PopCast
{
	class TInstance;
	class TInstanceRef;
	
	struct PluginParams;
	
	std::shared_ptr<TInstance>	Alloc(const TPopCastParams& Params);
	std::shared_ptr<TInstance>	GetInstance(TInstanceRef Instance);
	bool						Free(TInstanceRef Instance);
};



class PopCast::TInstanceRef
{
public:
	TInstanceRef() :
	mRef		( 0 )
	{
	}
	explicit TInstanceRef(Unity::ulong Ref) :
	mRef		( Ref )
	{
	}
	
	Unity::ulong	GetInt64() const	{	return mRef;	}
	bool			IsValid() const		{	return mRef != TInstanceRef().mRef;	}
	
	bool	operator==(const TInstanceRef& That) const	{	return mRef == That.mRef;	}
	bool	operator!=(const TInstanceRef& That) const	{	return mRef != That.mRef;	}
	
public:
	Unity::ulong	mRef;
};

class PopCast::TInstance
{
public:
	TInstance()=delete;
	TInstance(const TInstance& Copy)=delete;
public:
	explicit TInstance(const TInstanceRef& Ref,TPopCastParams Params);
	
	TInstanceRef	GetRef() const		{	return mRef;	}
	
public:
	std::shared_ptr<TCaster>	mCaster;
	
private:
	TInstanceRef	mRef;
};


