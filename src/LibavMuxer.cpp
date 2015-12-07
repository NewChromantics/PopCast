#include "LibavMuxer.h"
#include <SoyStream.h>
#include <SoyProtocol.h>
#include "TFileCaster.h"


class TRelay : public SoyWorkerThread
{
public:
	TRelay(std::shared_ptr<TStreamBuffer> Input,std::shared_ptr<TStreamWriter> Output);

	virtual bool	Iteration() override;
	virtual bool	CanSleep() override;
	
	std::shared_ptr<TStreamBuffer>	mInput;
	std::shared_ptr<TStreamWriter>	mOutput;
};


TRelay::TRelay(std::shared_ptr<TStreamBuffer> Input,std::shared_ptr<TStreamWriter> Output) :
	SoyWorkerThread		( "TRelay", SoyWorkerWaitMode::Wake ),
	mInput				( Input ),
	mOutput				( Output )
{
	WakeOnEvent( mInput->mOnDataPushed );
	Start();
}

bool TRelay::CanSleep()
{
	return mInput->IsEmpty();
}


bool TRelay::Iteration()
{
	if ( mInput->IsEmpty() )
		return true;
	
	//	pop some data
	std::shared_ptr<TRawWriteDataProtocol> WriteData( new TRawWriteDataProtocol );
	auto& Buffer = WriteData->mData;
	static size_t ReadSizeMax = 1 * 1024 * 1024;
	size_t ReadSize = std::min( mInput->GetBufferedSize(), ReadSizeMax );
	if ( ReadSize == 0 )
		return true;
	
	mInput->Pop( ReadSize, GetArrayBridge(Buffer) );
	
	//	pass it on
	mOutput->Push( WriteData );
	
	return true;
}


Libav::TMuxer::TMuxer(std::shared_ptr<TStreamWriter>& Output,std::shared_ptr<TMediaPacketBuffer>& Input) :
	TMediaMuxer	( Output, Input, "Libav::TMuxer" ),
	mLibavOutput	( new TStreamBuffer )
{
	mContext.reset( new Libav::TContext("ts",mLibavOutput) );
	
	//	capture mLibavOutput and turn into a raw packet to send to the stream writer...
	//	gr: see if the base class might want to re-use this buffer rather than deal as packets just because all muxers will otuput raw data?
	mLibavToOutputRelay.reset( new TRelay( mLibavOutput, Output ) );
}


void Libav::TMuxer::SetupStreams(const ArrayBridge<TStreamMeta>&& Streams)
{
	mContext->WriteHeader( Streams );
}

void Libav::TMuxer::ProcessPacket(std::shared_ptr<TMediaPacket> Packet,TStreamWriter& Output)
{
	//	push an av packet.
	//	note; we won't write straight to this TStreamWriter, so maybe the base class needs to be reworked for async writes
	//	OR the AvWritePacket needs to use a callback (as this is synchronous anyway)
	Libav::TPacket PacketLibav( Packet );
	mContext->WritePacket( PacketLibav );
}

