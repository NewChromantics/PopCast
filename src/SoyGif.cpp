#include "SoyGif.h"
#include "TFileCaster.h"



std::shared_ptr<TMediaEncoder> Gif::AllocEncoder(std::shared_ptr<TMediaPacketBuffer>& OutputBuffer,size_t StreamIndex)
{
	std::shared_ptr<TMediaEncoder> Encoder( new TEncoder( OutputBuffer, StreamIndex ) );
	return Encoder;
}



Gif::TMuxer::TMuxer(std::shared_ptr<TStreamWriter>& Output,std::shared_ptr<TMediaPacketBuffer>& Input,const std::string& ThreadName) :
	TMediaMuxer		( Output, Input, ThreadName )
{
}
	
void Gif::TMuxer::Finish()
{
	//	write gif tail byte
}

void Gif::TMuxer::SetupStreams(const ArrayBridge<TStreamMeta>&& Streams)
{
	Soy::Assert( Streams.GetSize() == 1, "Gif must have one stream only" );

	//	write out header
}

void Gif::TMuxer::ProcessPacket(std::shared_ptr<TMediaPacket> Packet,TStreamWriter& Output)
{
	//	write immediately if data is gif
	Soy::Assert( Packet->mMeta.mCodec == SoyMediaFormat::GifFrame, "Gif muxer can only eat gif frame data");

	std::shared_ptr<TRawWritePacketProtocol> Protocol( new TRawWritePacketProtocol( Packet ) );
	Output.Push( Protocol );
}



Gif::TEncoder::TEncoder(std::shared_ptr<TMediaPacketBuffer>& OutputBuffer,size_t StreamIndex) :
	TMediaEncoder	( OutputBuffer ),
	mStreamIndex	( StreamIndex )
{
}
	
void Gif::TEncoder::Write(const Opengl::TTexture& Image,SoyTime Timecode,Opengl::TContext& Context)
{
	//	do here in opengl...
	//		determine palette for frame (or predefined palette)
	//		reduce image to 256 colours (with dither/closest shader)
	//		read pixels
	
	//	do on thread
	//		encode to gif format
	//		queue packet
}

void Gif::TEncoder::Write(const std::shared_ptr<SoyPixelsImpl> Image,SoyTime Timecode)
{
}

