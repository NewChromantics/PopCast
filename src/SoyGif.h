#pragma once

#include <SoyTypes.h>
#include <SoyMedia.h>


namespace Gif
{
	std::shared_ptr<TMediaEncoder>	AllocEncoder(std::shared_ptr<TMediaPacketBuffer>& OutputBuffer,size_t StreamIndex);
	
	class TMuxer;
	class TEncoder;
}

class Gif::TMuxer : public TMediaMuxer
{
public:
	TMuxer(std::shared_ptr<TStreamWriter>& Output,std::shared_ptr<TMediaPacketBuffer>& Input,const std::string& ThreadName);
	
protected:
	virtual void			Finish() override;
	virtual void			SetupStreams(const ArrayBridge<TStreamMeta>&& Streams) override;
	virtual void			ProcessPacket(std::shared_ptr<TMediaPacket> Packet,TStreamWriter& Output) override;

public:
};


class Gif::TEncoder : public TMediaEncoder
{
public:
	TEncoder(std::shared_ptr<TMediaPacketBuffer>& OutputBuffer,size_t StreamIndex);
	
	virtual void					Write(const Opengl::TTexture& Image,SoyTime Timecode,Opengl::TContext& Context) override;
	virtual void					Write(const std::shared_ptr<SoyPixelsImpl> Image,SoyTime Timecode) override;
	
public:
	size_t		mStreamIndex;
};
