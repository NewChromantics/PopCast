#pragma once

#include <SoyTypes.h>
#include <SoyThread.h>
#include "PopUnity.h"


class TCasterParams
{
public:
	std::string		mName;		//	filename, device name etc
	SoyPixelsMeta	mMeta;		//	if we know ahead of time the format of what we're outputting we can pre-prepare
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
	//	throw if your caster can't support these
	virtual void		Write(const Opengl::TTexture& Image,const TCastFrameMeta& Frame,Opengl::TContext& Context)=0;
	virtual void		Write(const std::shared_ptr<SoyPixelsImpl> Image,const TCastFrameMeta& Frame)=0;
};



