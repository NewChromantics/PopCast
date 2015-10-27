#pragma once

#include "TCaster.h"
#include <SoyMedia.h>


namespace Avf
{
	class TEncoder;		//	SoyMediaEncoder
	class TSession;		//	objc interface
	class TParams;
};


namespace H264ProfileLevel
{
	enum Type
	{
		Invalid,
		Baseline,
	};
	
	DECLARE_SOYENUM(H264ProfileLevel);
}



class Avf::TParams
{
public:
	TParams() :
		mAverageBitrate		( 1000 ),
		mProfile			( H264ProfileLevel::Baseline ),
		mRealtimeEncoding	( true ),
		mRequireHardwareAcceleration	( false ),
		mMaxKeyframeInterval	( 2000ull )
	{
	}

public:
	bool					mRequireHardwareAcceleration;
	size_t					mAverageBitrate;
	H264ProfileLevel::Type	mProfile;
	bool					mRealtimeEncoding;
	SoyTime					mMaxKeyframeInterval;	//	Videotoolbox actually restricts this per-second. 0 is no-limit
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


