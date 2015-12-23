#include "SoyGif.h"
#include "TFileCaster.h"

#include "gif.h"

namespace Gif
{
	void	MakeIndexedImage(SoyPixelsImpl& IndexedImage,const SoyPixelsImpl& Rgba);
}

std::shared_ptr<TMediaEncoder> Gif::AllocEncoder(std::shared_ptr<TMediaPacketBuffer>& OutputBuffer,size_t StreamIndex)
{
	std::shared_ptr<TMediaEncoder> Encoder( new TEncoder( OutputBuffer, StreamIndex ) );
	return Encoder;
}



Gif::TMuxer::TMuxer(std::shared_ptr<TStreamWriter>& Output,std::shared_ptr<TMediaPacketBuffer>& Input,const std::string& ThreadName) :
	TMediaMuxer		( Output, Input, ThreadName )
{
}

Gif::TMuxer::~TMuxer()
{
	std::lock_guard<std::mutex> Lock( mBusy );
	
}

void Gif::TMuxer::Finish()
{
	std::lock_guard<std::mutex> Lock( mBusy );
	GifEnd( *mWriter );
	mWriter.reset();
}


void Gif::TMuxer::WriteToBuffer(const ArrayBridge<uint8>&& Data)
{
	if ( !mBuffer )
	{
		mBuffer.reset( new TRawWriteDataProtocol );
	}
	auto& Buffer = mBuffer->mData;

	Buffer.PushBackArray( Data );
	
	//	flush
	static int FlushSize = 1024 * 64;
	if ( Buffer.GetDataSize() > FlushSize )
		FlushBuffer();
}

void Gif::TMuxer::FlushBuffer()
{
	if ( !mBuffer )
		return;
	
	mOutput->Push( mBuffer );
	mBuffer.reset();
}
	
void Gif::TMuxer::SetupStreams(const ArrayBridge<TStreamMeta>&& Streams)
{
	std::lock_guard<std::mutex> Lock( mBusy );
	Soy::Assert( Streams.GetSize() == 1, "Gif must have one stream only" );

	auto Open = [this]
	{
		
	};
	auto Putc = [this](uint8 c)
	{
		auto Data = GetRemoteArray( &c, 1 );
		WriteToBuffer( GetArrayBridge(Data) );
	};
	auto Puts = [this](const char* s)
	{
		size_t Size = 0;
		while ( s[Size] )
			Size++;
		
		auto Data = GetRemoteArray( reinterpret_cast<const uint8*>(s), Size );
		WriteToBuffer( GetArrayBridge(Data) );
	};
	auto fwrite = [this](uint8* Buffer,size_t Length)
	{
		auto Data = GetRemoteArray( Buffer, Length );
		WriteToBuffer( GetArrayBridge(Data) );
	};
	auto Close = [this]
	{
		FlushBuffer();
	};
	
	if ( !mWriter )
	{
		mWriter.reset( new GifWriter );
		mWriter->Open = Open;
		mWriter->fputc = Putc;
		mWriter->fputs = Puts;
		mWriter->fwrite = fwrite;
		mWriter->Close = Close;
	}

	auto PixelMeta = Streams[0].mPixelMeta;
	auto Delay = 8;	//	Packet->mDuration
	GifBegin( *mWriter, PixelMeta.GetWidth(), PixelMeta.GetHeight(), Delay );
}


void Gif::MakeIndexedImage(SoyPixelsImpl& IndexedImage,const SoyPixelsImpl& RgbaImage)
{
	IndexedImage.Init( RgbaImage.GetWidth(), RgbaImage.GetHeight(), SoyPixelsFormat::Greyscale );
	auto& IndexPixels = IndexedImage.GetPixelsArray();
	auto& RgbaPixels = RgbaImage.GetPixelsArray();
	
	for ( int p=0;	p<IndexPixels.GetSize();	p++ )
	{
		auto Alpha = RgbaPixels[ p*4 + 3 ];
		auto PaletteIndex = Alpha;
		IndexPixels[p] = PaletteIndex;
	}
}

void Gif::TMuxer::ProcessPacket(std::shared_ptr<TMediaPacket> Packet,TStreamWriter& Output)
{
	std::lock_guard<std::mutex> Lock( mBusy );
	
	if ( !mWriter )
		return;

	auto delay = 8;	//	Packet->mDuration
	auto width = Packet->mMeta.mPixelMeta.GetWidth();
	auto height = Packet->mMeta.mPixelMeta.GetHeight();
	bool Dither = false;
	auto Channels = SoyPixelsFormat::GetChannelCount( Packet->mMeta.mPixelMeta.GetFormat() );
	
	Soy::TScopeTimerPrint Timer("GifWriteFrame",1);
	auto* RgbaData = Packet->mData.GetArray();
	auto* image = RgbaData;
	
	GifWriter& writer = *mWriter;

	
	std::shared_ptr<GifPalette> pNewPalette( new GifPalette );
	auto& NewPalette = *pNewPalette;
	
	{
		//	slow ~100ms
		Soy::TScopeTimerPrint Timer("GifMakePalette",1);
		GifMakePalette((Dither? NULL : mPrevImage.get()), (Dither? NULL : mPrevPalette.get()), image, width, height, Dither, NewPalette);
	}
	
	std::shared_ptr<SoyPixels> pIndexedImage( new SoyPixels );
	auto& IndexedImage = *pIndexedImage;
	if(Dither)
	{
		Soy::TScopeTimerPrint Timer("GifDitherImage",1);
		SoyPixels ReducedImage;
		ReducedImage.Init( width, height, SoyPixelsFormat::RGBA );
		GifDitherImage( mPrevImage.get(), mPrevPalette.get(), image, ReducedImage, width, height, NewPalette);
		MakeIndexedImage( IndexedImage, ReducedImage );
	}
	else
	{
		//	slower ~200ms
		Soy::TScopeTimerPrint Timer("GifThresholdImage",1);
		IndexedImage.Init( width, height, SoyPixelsFormat::Greyscale );
		GifThresholdImage( mPrevImage.get(), mPrevPalette.get(), image, IndexedImage, width, height, NewPalette);
	}

	{
		//	fastish ~7ms
		Soy::TScopeTimerPrint Timer("GifWriteLzwImage",1);
		GifWriteLzwImage(writer, IndexedImage, 0, 0, delay, NewPalette);
	}
	
	mPrevImage = pIndexedImage;
	mPrevPalette = pNewPalette;
	
	std::Debug << "GifWriteFrameFinished" << std::endl;
}



Gif::TEncoder::TEncoder(std::shared_ptr<TMediaPacketBuffer>& OutputBuffer,size_t StreamIndex) :
	SoyWorkerThread	( "Gif::TEncoder", SoyWorkerWaitMode::Wake ),
	TMediaEncoder	( OutputBuffer ),
	mStreamIndex	( StreamIndex )
{
	Start();
}
	
void Gif::TEncoder::Write(const Opengl::TTexture& Image,SoyTime Timecode,Opengl::TContext& Context)
{
	//	do here in opengl...
	//		determine palette for frame (or predefined palette)
	//		reduce image to 256 colours (with dither/closest shader)
	//		read pixels
	std::shared_ptr<TMediaPacket> pPacket( new TMediaPacket );
	auto& Packet = *pPacket;
	
	//	construct pixels against data and read it
	SoyPixelsDef<Array<uint8>> Pixels( Packet.mData, Packet.mMeta.mPixelMeta );
	Image.Read( Pixels );
	
	Packet.mTimecode = Timecode;
	Packet.mMeta.mCodec = SoyMediaFormat::FromPixelFormat( Packet.mMeta.mPixelMeta.GetFormat() );

	PushFrame( pPacket );
	
	//	do on thread
	//		encode to gif format
	//		queue packet
}

void Gif::TEncoder::Write(const std::shared_ptr<SoyPixelsImpl> Image,SoyTime Timecode)
{
	throw Soy::AssertException("Currently only supporting opengl");
}

bool Gif::TEncoder::CanSleep()
{
	return mPendingFrames.GetSize() == 0;
}

bool Gif::TEncoder::Iteration()
{
	auto pPacket = PopFrame();
	if ( !pPacket )
		return true;
	auto& Packet = *pPacket;

	//	drop packets
	auto Block = []
	{
		return false;
	};

	TMediaEncoder::PushFrame( pPacket, Block );
	return true;
}


void Gif::TEncoder::PushFrame(std::shared_ptr<TMediaPacket> Frame)
{
	std::lock_guard<std::mutex> Lock( mPendingFramesLock );
	mPendingFrames.PushBack( Frame );
	Wake();
}


std::shared_ptr<TMediaPacket> Gif::TEncoder::PopFrame()
{
	if ( mPendingFrames.IsEmpty() )
		return nullptr;
	
	std::lock_guard<std::mutex> Lock( mPendingFramesLock );
	return mPendingFrames.PopAt(0);
}

