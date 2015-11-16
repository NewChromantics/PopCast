#pragma once

#include <SoyMedia.h>



namespace Libav
{
	class TExtractor;	//	demuxer
	
	//	AVERROR_xxx
	bool	IsOkay(int Error,const std::string& Context,bool Throw=true);
};



class Libav::TExtractor : public TMediaExtractor
{
public:
	TExtractor(std::shared_ptr<TStreamBuffer> InputStream,SoyEvent<const SoyTime>& OnFrameExtractedEvent);
	
	virtual void									GetStreams(ArrayBridge<TStreamMeta>&& Streams) override;
	virtual std::shared_ptr<Platform::TMediaFormat>	GetStreamFormat(size_t StreamIndex) override;

protected:
	virtual std::shared_ptr<TMediaPacket>	ReadNextPacket() override;
	

protected:
	std::shared_ptr<TStreamBuffer>	mInputStream;
};


