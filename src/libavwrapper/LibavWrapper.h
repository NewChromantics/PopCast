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
	
	void		IsOkay(int LibavError,const std::string& Context,bool Throw=true);
};



class Libav::TContext
{
public:
	TContext(const std::string& FormatName,std::shared_ptr<TStreamBuffer> Output);
	
	void		WriteHeader(const ArrayBridge<TStreamMeta>& Streams);
	void		WritePacket(const Libav::TPacket& Packet);

private:
	std::shared_ptr<AVPacket>			mAvPacket;
	std::shared_ptr<AVFormatContext>	mFormat;
	std::shared_ptr<AVIOContext>		mIo;
	std::shared_ptr<TStreamBuffer>		mOutput;
};


class Libav::TPacket
{
public:
	TPacket(std::shared_ptr<TMediaPacket> Packet);
	
public:
	std::shared_ptr<AVPacket>		mAvPacket;
	std::shared_ptr<TMediaPacket>	mSoyPacket;
};



