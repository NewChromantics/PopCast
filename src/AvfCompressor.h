#pragma once

#include "TCaster.h"
#include <SoyMedia.h>
#include <SoyH264.h>


namespace Avf
{
	class TEncoder;		//	SoyMediaEncoder
	class TPassThroughEncoder;
	class TSession;		//	objc interface
	class TParams;
};


class Avf::TParams
{
public:
	TParams() :
		mAverageBitrate		( 1000 ),
		mProfile			( H264Profile::Baseline ),
		mLevel				( 1, 0 ),
		mRealtimeEncoding	( true ),
		mRequireHardwareAcceleration	( false ),
		mMaxKeyframeInterval	( 2000ull )
	{
	}

public:
	bool					mRequireHardwareAcceleration;
	size_t					mAverageBitrate;
	bool					mRealtimeEncoding;
	SoyTime					mMaxKeyframeInterval;	//	Videotoolbox actually restricts this per-second. 0 is no-limit

	H264Profile::Type		mProfile;
	Soy::TVersion			mLevel;
};



class Avf::TEncoder : public TMediaEncoder
{
public:
	TEncoder(const TCasterParams& Params,std::shared_ptr<TMediaPacketBuffer>& OutputBuffer,size_t StreamIndex);
	
	virtual void		Write(const Opengl::TTexture& Image,SoyTime Timecode,Opengl::TContext& Context) override;
	virtual void		Write(const std::shared_ptr<SoyPixelsImpl> Image,SoyTime Timecode) override;

	void				OnError(const std::string& Error);
	
public:
	std::shared_ptr<TSession>	mSession;
	SoyPixelsMeta				mOutputMeta;
	TMediaPacketBuffer			mFrames;
};



class Avf::TPassThroughEncoder : public TMediaEncoder
{
public:
	TPassThroughEncoder(const TCasterParams& Params,std::shared_ptr<TMediaPacketBuffer>& OutputBuffer,size_t StreamIndex);
	
	virtual void		Write(const Opengl::TTexture& Image,SoyTime Timecode,Opengl::TContext& Context) override;
	virtual void		Write(const std::shared_ptr<SoyPixelsImpl> Image,SoyTime Timecode) override;
	
	void				OnError(const std::string& Error);
	
public:
	std::shared_ptr<TSession>	mSession;
	SoyPixelsMeta				mOutputMeta;
	TMediaPacketBuffer			mFrames;
	size_t						mStreamIndex;	//	may want to be in base
};



