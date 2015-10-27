#include "TFileCaster.h"
#include <SoyMedia.h>
#include "AvfCompressor.h"


TFileCaster::TFileCaster(const TCasterParams& Params) :
	SoyWorkerThread	( std::string("TFileCaster/")+Params.mName, SoyWorkerWaitMode::Wake )
{
	auto& Filename = Params.mName;
	
	//	open file
	mFileStream = std::ofstream( Filename, std::ios::out );
	Soy::Assert( mFileStream.is_open(), std::string("Failed to open ")+Filename );
	
	//	alloc & listen for new packets
	mFrameBuffer.reset( new TMediaPacketBuffer() );
	this->WakeOnEvent( mFrameBuffer->mOnNewPacket );
	
	//	alloc encoder
	size_t StreamIndex = 0;
	mEncoder.reset( new Avf::TEncoder( Params, mFrameBuffer, StreamIndex ) );
	
	Start();
}

TFileCaster::~TFileCaster()
{
	//	wait for encoder
	mEncoder.reset();

	WaitToFinish();
	mFileStream.close();
	
	mFrameBuffer.reset();
}

bool TFileCaster::Iteration()
{
	try
	{
		//	pop frames
		auto pPacket = mFrameBuffer->PopPacket();
		if ( !pPacket )
			return true;

		auto& Packet = *pPacket;
	
		//	write frame
		auto* Data = reinterpret_cast<const char*>( Packet.mData.GetArray() );
		size_t DataSize = Packet.mData.GetDataSize();
		mFileStream.write( Data, DataSize );
	
		//Soy::Assert( File.fail(), "Error writing to file" );
		Soy::Assert( !mFileStream.bad(), "Error writing to file" );
	}
	catch (std::exception& e)
	{
		std::Debug << "TFileCaster error: " << e.what() << std::endl;
	}
	
	return true;
}


bool TFileCaster::CanSleep()
{
	if ( !mFrameBuffer )
		return true;
	
	return mFrameBuffer->HasPackets();
}

void TFileCaster::Write(const Opengl::TTexture& Image,SoyTime Timecode,Opengl::TContext& Context)
{
	Soy::Assert( mEncoder!=nullptr, "Missing encoder" );
	mEncoder->Write( Image, Timecode, Context );
}

void TFileCaster::Write(const std::shared_ptr<SoyPixelsImpl> Image,SoyTime Timecode)
{
	Soy::Assert( mEncoder!=nullptr, "Missing encoder" );
	mEncoder->Write( Image, Timecode );
}




