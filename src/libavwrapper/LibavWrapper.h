#pragma once

#include <SoyMedia.h>

extern "C"
{
	struct AVPacket;
	struct AVFormatContext;
	struct AVIOContext;
};

//	our c++ interface for generic stuff
namespace Libav
{
	class TContext;
	class TPacket;

	template<typename TYPE>
	class TAvWrapperBase;
	template<typename TYPE>
	class TAvWrapper;
	
	void		IsOkay(int LibavError,const std::string& Context,bool Throw=true);
	
	
};


class Libav::TContext
{
public:
	TContext(const std::string& FormatName,std::shared_ptr<TStreamBuffer> Output);
	
	void		WriteHeader(const ArrayBridge<TStreamMeta>& Streams);
	void		WritePacket(const Libav::TPacket& Packet);

	size_t		IoTell();
	void		IoFlush();
	void		IoWrite(const ArrayBridge<uint8>&& Data);
	
private:
	std::shared_ptr<TAvWrapper<AVFormatContext>>	mFormat;
	std::shared_ptr<TStreamBuffer>		mOutput;
};


class Libav::TPacket
{
public:
	TPacket(std::shared_ptr<TMediaPacket> Packet);
	
public:
	std::shared_ptr<TAvWrapper<AVPacket>>	mAvPacket;
	std::shared_ptr<TMediaPacket>			mSoyPacket;
};



template<typename TYPE>
class Libav::TAvWrapperBase
{
public:
	TAvWrapperBase()
	{
		memset( &mObject, 0, sizeof(mObject) );
	}
	
	TYPE*	GetObject()	{	return &mObject;	}
	
public:
	TYPE	mObject;
};


