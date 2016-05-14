#pragma once

#include <SoyTypes.h>
#include <SoyThread.h>
#include "PopUnity.h"
#include "SoyGif.h"
#include <SoyH264.h>



class TCasterDeviceParams
{
public:
	std::shared_ptr<Opengl::TContext>			OpenglContext;
	std::shared_ptr<TPool<Opengl::TTexture>>	OpenglTexturePool;

#if defined(ENABLE_DIRECTX)
	std::shared_ptr<Directx::TContext>			DirectxContext;
	std::shared_ptr<TPool<Directx::TTexture>>	DirectxTexturePool;
#endif
};



class TCasterParams
{
public:
	TCasterParams() :
		mShowFinishedFile	( false ),
		mSkipFrames			( false )
	{
	}
	
public:
	std::string			mName;				//	filename, device name etc
	bool				mShowFinishedFile;
	bool				mSkipFrames;
	Gif::TEncodeParams	mGifParams;
	TMediaEncoderParams	mMpegParams;
	size_t				mMaxSeconds;
	size_t				mMaxKiloBytes;
};

class TCastDeviceMeta
{
public:
	bool			IsValid() const	{	return !mName.empty();	}
	
	std::string		mName;
	std::string		mAddress;
	std::string		mSerial;
	std::string		mVendor;
	std::string		mModel;
};
inline std::ostream& operator<<(std::ostream &out,const TCastDeviceMeta&in)
{
	out << in.mName << "@" << in.mAddress << "/" << in.mModel << "/" << in.mVendor << "(" << in.mSerial << ")";
	return out;
}

class TCastFrameMeta
{
public:
	TCastFrameMeta() :
		mStreamIndex	( 0 )
	{
	}
	TCastFrameMeta(SoyTime Timecode,size_t StreamIndex) :
		mTimecode		( Timecode ),
		mStreamIndex	( StreamIndex )
	{
		Soy::Assert( Timecode.IsValid(), "Should not pass invalid timecodes here");
	}
public:
	SoyTime		mTimecode;
	size_t		mStreamIndex;
};

class TCaster
{
public:
	TCaster(const TCasterParams& Params) :
		mParams		( Params )
	{
	}
	
	//	throw if your caster can't support these
	virtual void		Write(const Opengl::TTexture& Image,const TCastFrameMeta& Frame,Opengl::TContext& Context)=0;
	virtual void		Write(const Directx::TTexture& Image,const TCastFrameMeta& Frame,Directx::TContext& Context)=0;
	virtual void		Write(std::shared_ptr<SoyPixelsImpl> Image,const TCastFrameMeta& Frame)=0;
	virtual void		GetMeta(TJsonWriter& Json) {}
	virtual size_t		GetPendingPacketCount()	{	throw Soy::AssertException("Needs implementing");	}

	const TCasterParams&	GetParams() const	{	return mParams;	}

protected:
	TCasterParams	mParams;
};



