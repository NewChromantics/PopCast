#include "LibavMuxer.h"
#include <SoyStream.h>


Libav::TMuxer::TMuxer(std::shared_ptr<TStreamWriter>& Output,std::shared_ptr<TMediaPacketBuffer>& Input) :
	TMediaMuxer	( Output, Input, "Libav::TMuxer" ),
	mLibavOutput	( new TStreamBuffer )
{
	mContext.reset( new Libav::TContext("ts",mLibavOutput) );
	
	//	capture mLibavOutput and turn into a raw packet to send to the stream writer...
	//	gr: see if the base class might want to re-use this buffer rather than deal as packets just because all muxers will otuput raw data?
}


void Libav::TMuxer::SetupStreams(const ArrayBridge<TStreamMeta>&& Streams)
{
	mContext->WriteHeader( Streams );
}

void Libav::TMuxer::ProcessPacket(std::shared_ptr<TMediaPacket> Packet,TStreamWriter& Output)
{
	//	make an av packet
	Libav::TPacket PacketLibav( Packet );
	mContext->WritePacket( PacketLibav );
}

