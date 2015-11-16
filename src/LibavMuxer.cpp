#include "LibavMuxer.h"




Libav::TExtractor::TExtractor(std::shared_ptr<TStreamBuffer> InputStream,SoyEvent<const SoyTime>& OnFrameExtractedEvent) :
	TMediaExtractor		( std::string("Libav::TExtractor/"), OnFrameExtractedEvent ),
	mInputStream		( InputStream )
{
	
}


std::shared_ptr<TMediaPacket> Libav::TExtractor::ReadNextPacket()
{
	// av_read_frame(ctx, &pkt);
	return nullptr;
}

void Libav::TExtractor::GetStreams(ArrayBridge<TStreamMeta>&& Streams)
{
	
}

std::shared_ptr<Platform::TMediaFormat> Libav::TExtractor::GetStreamFormat(size_t StreamIndex)
{
	return nullptr;
}


