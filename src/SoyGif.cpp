/*
 
 
 
 int Watermark_00 = 0x7FFF; int Watermark_10 = 0xFFFC;
 int Watermark_01 = 0xC444; int Watermark_11 = 0x4446;
 int Watermark_02 = 0xD555;	int Watermark_12 = 0xD5EE;
 int Watermark_03 = 0xC545;	int Watermark_13 = 0xC46E;
 int Watermark_04 = 0xDD5D;	int Watermark_14 = 0xD76E;
 int Watermark_05 = 0xDC5D;	int Watermark_15 = 0x546E;
 int Watermark_06 = 0x7FFF;	int Watermark_16 = 0xFFFC;
 int Watermark_07 = 0xE00;	int Watermark_17 = 0;
 int Watermark_08 = 0xC00;	int Watermark_18 = 0;
 int Watermark_09 = 0x1800;	int Watermark_19 = 0;
 
 #define BIT_MAX 16
 
 int bitwiseAnd(int a, int bit)
 {
 if ( bit > BIT_MAX ) return 0;
 bit = BIT_MAX-bit;
 int b = 1;
 for ( int i=1;i<BIT_MAX;i++ )
 {
 if ( bit > 0 )
 {
 b *= 2;
 bit -= 1;
 }
 }
 
 int result = 0;
 int n = 1;
 for ( int i=0; i<BIT_MAX; i++ )
 {
 if ((a > 0) && (b > 0)) {
 if (((mod(float(a),2.0)) == 1.0) && ((mod(float(b),2.0)) == 1.0)) {
 result += n;
 }
 a = a / 2;
 b = b / 2;
 n = n * 2;
 }
 else
 break;
 }
 return result;
 }
 
 
 int GetWatermark(int x, int y)
 {
 if ( x < 0 )
 return 0;
 if ( y == 0 )	return x < 16 ? Watermark_00 : Watermark_10;
 if ( y == 1 )	return x < 16 ? Watermark_01 : Watermark_11;
 if ( y == 2 )	return x < 16 ? Watermark_02 : Watermark_12;
 if ( y == 3 )	return x < 16 ? Watermark_03 : Watermark_13;
 if ( y == 4 )	return x < 16 ? Watermark_04 : Watermark_14;
 if ( y == 5 )	return x < 16 ? Watermark_05 : Watermark_15;
 if ( y == 6 )	return x < 16 ? Watermark_06 : Watermark_16;
 if ( y == 7 )	return x < 16 ? Watermark_07 : Watermark_17;
 if ( y == 8 )	return x < 16 ? Watermark_08 : Watermark_18;
 if ( y == 9 )	return x < 16 ? Watermark_09 : Watermark_19;
 
 return 0;
 }
 
 void mainImage( out vec4 fragColor, in vec2 fragCoord )
 {
 vec2 uv = fragCoord.xy / iResolution.xy;
	fragColor = vec4(uv,0.5+0.5*sin(iGlobalTime),1.0);
 
 
 
 int x = int(floor(uv.x * 60.0) )- 10;
 int y = int((1.0-uv.y) * 60.0 * (iResolution.y/iResolution.x)) - 2;
 
 
 int Watermark = GetWatermark( x, y );
	bool WatermarkPixel = bitwiseAnd( Watermark, x<16 ? x : x-16 ) != 0;
 
 if ( WatermarkPixel )
 fragColor = vec4( 1,1,1,1 );
 
 }
 */


#include "SoyGif.h"
#include "TFileCaster.h"

#include "gif.h"

//	watermark
//	https://www.shadertoy.com/view/ld33zX

namespace Gif
{
	void	MakeIndexedImage(SoyPixelsImpl& IndexedImage,const SoyPixelsImpl& Rgba);
}

std::shared_ptr<TMediaEncoder> Gif::AllocEncoder(std::shared_ptr<TMediaPacketBuffer>& OutputBuffer,size_t StreamIndex,std::shared_ptr<Opengl::TContext> OpenglContext)
{
	std::shared_ptr<TMediaEncoder> Encoder( new TEncoder( OutputBuffer, StreamIndex, OpenglContext ) );
	return Encoder;
}



Gif::TMuxer::TMuxer(std::shared_ptr<TStreamWriter>& Output,std::shared_ptr<TMediaPacketBuffer>& Input,const std::string& ThreadName) :
	TMediaMuxer		( Output, Input, std::string("Gif::TMuxer ")+ThreadName ),
	mFinished		( false ),
	mStarted		( false )
{
}

Gif::TMuxer::~TMuxer()
{
	mBusy.lock();
	if ( !mFinished )
	{
		std::Debug << "TMuxer destructor not finished" << std::endl;
	}
	mBusy.unlock();
	WaitToFinish();
}

void Gif::TMuxer::Finish()
{
	std::lock_guard<std::mutex> Lock( mBusy );

	if ( mFinished )
		return;
	if ( !mStarted )
		return;
	
	mFinished = true;
	
	std::shared_ptr<Soy::TWriteProtocol> FooterWrite( new TRawWriteDataProtocol );
	Array<char>& HeaderData = dynamic_cast<TRawWriteDataProtocol&>( *FooterWrite ).mData;
	
	auto Open = []
	{
		
	};
	auto Putc = [this,&HeaderData](uint8 c)
	{
		HeaderData.PushBack(c);
	};
	auto Puts = [this,&HeaderData](const char* s)
	{
		size_t Size = 0;
		while ( s[Size] )
			Size++;
		
		auto Data = GetRemoteArray( s, Size );
		HeaderData.PushBackArray( Data );
	};
	auto fwrite = [this,&HeaderData](uint8* Buffer,size_t Length)
	{
		auto Data = GetRemoteArray( reinterpret_cast<const char*>(Buffer), Length );
		HeaderData.PushBackArray( Data );
	};
	
	GifWriter Writer;
	Writer.Open = Open;
	Writer.fputc = Putc;
	Writer.fputs = Puts;
	Writer.fwrite = fwrite;
	

	GifEnd( Writer );
	
	mOutput->Push( FooterWrite );
}


void Gif::TMuxer::SetupStreams(const ArrayBridge<TStreamMeta>&& Streams)
{
	std::lock_guard<std::mutex> Lock( mBusy );
	Soy::Assert( Streams.GetSize() == 1, "Gif must have one stream only" );

	if ( mStarted )
	{
		std::Debug << __func__ << " already executed" << std::endl;
		return;
	}
	
	std::shared_ptr<Soy::TWriteProtocol> HeaderWrite( new TRawWriteDataProtocol );
	Array<char>& HeaderData = dynamic_cast<TRawWriteDataProtocol&>( *HeaderWrite ).mData;
	
	auto Open = []
	{
		
	};
	auto Putc = [this,&HeaderData](uint8 c)
	{
		HeaderData.PushBack(c);
	};
	auto Puts = [this,&HeaderData](const char* s)
	{
		size_t Size = 0;
		while ( s[Size] )
			Size++;
		
		auto Data = GetRemoteArray( s, Size );
		HeaderData.PushBackArray( Data );
	};
	auto fwrite = [this,&HeaderData](uint8* Buffer,size_t Length)
	{
		auto Data = GetRemoteArray( reinterpret_cast<const char*>(Buffer), Length );
		HeaderData.PushBackArray( Data );
	};
	
	GifWriter Writer;
	Writer.Open = Open;
	Writer.fputc = Putc;
	Writer.fputs = Puts;
	Writer.fwrite = fwrite;

	auto PixelMeta = Streams[0].mPixelMeta;
	auto Delay = 8;	//	Packet->mDuration
	GifBegin( Writer, PixelMeta.GetWidth(), PixelMeta.GetHeight(), Delay );

	mOutput->Push( HeaderWrite );
	
	mStarted = true;
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
	Soy::Assert( Packet!=nullptr, "Expected packet");
	std::lock_guard<std::mutex> Lock( mBusy );
	
	if ( !mStarted )
	{
		std::Debug << "Gif muxer not started, dropping packet " << *Packet << std::endl;
		return;
	}
	if ( mFinished )
	{
		std::Debug << "Gif muxer finished, dropping packet " << *Packet << std::endl;
		return;
	}

	SoyPixelsDef<Array<uint8>> PalettisedImage( Packet->mData, Packet->mMeta.mPixelMeta );
	Soy::Assert( Packet->mMeta.mCodec == SoyMediaFormat::Palettised_8_8, "Expected palettised image as codec");
	Soy::Assert( PalettisedImage.GetFormat() == SoyPixelsFormat::Palettised_8_8, "Expected palettised image in pixel meta");
	
	//	ms to 100th's
	auto delay = std::max<int>( 1, Packet->mDuration.GetTime() / 100 );
	

	{
		GifWriter LzwWriter;
		
		std::shared_ptr<Soy::TWriteProtocol> LzwWrite( new TRawWriteDataProtocol );
		Array<char>& LzwData = dynamic_cast<TRawWriteDataProtocol&>( *LzwWrite ).mData;

		auto Putc = [this,&LzwData](uint8 c)
		{
			LzwData.PushBack(c);
		};
		auto Puts = [this,&LzwData](const char* s)
		{
			size_t Size = 0;
			while ( s[Size] )
				Size++;
			
			auto Data = GetRemoteArray( s, Size );
			LzwData.PushBackArray( Data );
		};
		auto fwrite = [this,&LzwData](uint8* Buffer,size_t Length)
		{
			auto Data = GetRemoteArray( reinterpret_cast<const char*>(Buffer), Length );
			LzwData.PushBackArray( Data );
		};
		
		LzwWriter.fputc = Putc;
		LzwWriter.fputs = Puts;
		LzwWriter.fwrite = fwrite;
		
		
		BufferArray<std::shared_ptr<SoyPixelsImpl>,2> PaletteAndIndexed;
		PalettisedImage.SplitPlanes( GetArrayBridge(PaletteAndIndexed) );
		
		auto& IndexedImage = *PaletteAndIndexed[1];
		auto& Palette = *PaletteAndIndexed[0];
		
		//	fastish ~7ms
		Soy::TScopeTimerPrint Timer("GifWriteLzwImage",1);
		GifWriteLzwImage( LzwWriter, IndexedImage, 0, 0, delay, Palette );
		
		Output.Push( LzwWrite );
	}
	
	std::Debug << "GifWriteFrameFinished" << std::endl;
}



Gif::TEncoder::TEncoder(std::shared_ptr<TMediaPacketBuffer>& OutputBuffer,size_t StreamIndex,std::shared_ptr<Opengl::TContext> OpenglContext) :
	SoyWorkerThread	( "Gif::TEncoder", SoyWorkerWaitMode::Wake ),
	TMediaEncoder	( OutputBuffer ),
	mStreamIndex	( StreamIndex ),
	mOpenglContext	( OpenglContext )
{
	Start();
}


Gif::TEncoder::~TEncoder()
{
	//	deffered delete of stuff
	if ( mOpenglContext )
	{
		mPendingFramesLock.lock();
		mPendingFrames.Clear();
		mPendingFramesLock.unlock();
		
		Opengl::DeferDelete( mOpenglContext, mOpenglBlitter );
		
		mOpenglContext.reset();
	}
}

void Gif::TEncoder::Write(const Opengl::TTexture& Image,SoyTime Timecode,Opengl::TContext& Context)
{
	//	do here in opengl...
	//		determine palette for frame (or predefined palette)
	//		reduce image to 256 colours (with dither/closest shader)
	//		read pixels
	std::shared_ptr<TMediaPacket> pPacket( new TMediaPacket );
	auto& Packet = *pPacket;

	static bool DupliateTexture = true;
	if ( DupliateTexture )
	{
		Packet.mPixelBuffer = CopyFrameImmediate( Image );
	}
	else
	{
		//	construct pixels against data and read it
		SoyPixelsDef<Array<uint8>> Pixels( Packet.mData, Packet.mMeta.mPixelMeta );
		Image.Read( Pixels );
	}
	Packet.mTimecode = Timecode;
	Packet.mMeta.mCodec = SoyMediaFormat::FromPixelFormat( Packet.mMeta.mPixelMeta.GetFormat() );
	
	PushFrame( pPacket );
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

	//	extract pixels
	SoyPixelsDef<Array<uint8>> Rgba( Packet.mData, Packet.mMeta.mPixelMeta );

	if ( Packet.mPixelBuffer )
	{
		auto& Texture = dynamic_cast<TTextureBuffer&>( *Packet.mPixelBuffer );
		auto Read = [&]
		{
			static bool Flip = false;
			Texture.mImage->Read( Rgba, SoyPixelsFormat::RGBA, Flip );
		};
		Soy::TSemaphore Semaphore;
		mOpenglContext->PushJob( Read, Semaphore );
		Semaphore.Wait();
		Packet.mPixelBuffer.reset();
	}
	
	//	make/find/copy palette
	//	reduce to 256
	//	mask
	//	push Palettised
	SoyPixels RgbaCopy;
	RgbaCopy.Copy( Rgba );
	SoyPixelsDef<Array<uint8>> PalettisedImage( Packet.mData, Packet.mMeta.mPixelMeta );
	MakePalettisedImage( PalettisedImage, RgbaCopy, Packet.mIsKeyFrame );

	Packet.mMeta.mCodec = SoyMediaFormat::FromPixelFormat( Packet.mMeta.mPixelMeta.GetFormat() );
	
	
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

std::shared_ptr<TTextureBuffer> Gif::TEncoder::CopyFrameImmediate(const Opengl::TTexture& Image)
{
	std::shared_ptr<TTextureBuffer> Buffer( new TTextureBuffer(mOpenglContext) );
	Buffer->mImage.reset( new Opengl::TTexture( Image.mMeta, GL_TEXTURE_2D ) );
	
	if ( !mOpenglBlitter )
		mOpenglBlitter.reset( new Opengl::TBlitter );

	BufferArray<Opengl::TTexture,1> ImageAsArray;
	ImageAsArray.PushBack( Image );
	Opengl::TTextureUploadParams Params;
	mOpenglBlitter->BlitTexture( *Buffer->mImage, GetArrayBridge(ImageAsArray), *mOpenglContext, Params );

	return Buffer;
}


void Gif::TEncoder::MakePalettisedImage(SoyPixelsImpl& PalettisedImage,const SoyPixelsImpl& Rgba,bool& Keyframe)
{
	auto width = Rgba.GetWidth();
	auto height = Rgba.GetHeight();
	static bool Dither = false;
	auto Channels = SoyPixelsFormat::GetChannelCount( PalettisedImage.GetFormat() );
	
	Soy::TScopeTimerPrint Timer("GifWriteFrame",1);
	auto& RgbaArray = PalettisedImage.GetPixelsArray();
	auto* RgbaData = RgbaArray.GetArray();
	auto* image = RgbaData;
	
	
	static bool TransparentMerge = false;
	
	auto pPrevIndexedImage = mPrevImageIndexes;
	auto pPrevPalette = mPrevPalette;

	if ( !TransparentMerge )
		pPrevIndexedImage.reset();

	static bool MakeDebugPalette = false;
		
	std::shared_ptr<GifPalette> pNewPalette;
	std::shared_ptr<SoyPixelsImpl> pNewPaletteImage;

	if ( MakeDebugPalette )
	{
		Keyframe = true;
		Soy::TScopeTimerPrint Timer("GifDebugPalette",1);
		GifDebugPalette( pNewPaletteImage );
	}
	else if ( !Dither && pPrevIndexedImage && pPrevPalette )
	{
		Keyframe = false;
		Soy::TScopeTimerPrint Timer("GifMakeDiffPalette",1);
		GifMakeDiffPalette( *pPrevIndexedImage, *pPrevPalette, image, width, height, pNewPaletteImage );
	}
	else
	{
		Keyframe = true;
		Soy::TScopeTimerPrint Timer("GifExtractPalette",1);
		GifExtractPalette( image, width, height, Channels, pNewPaletteImage );
	}
	
	//	make palette lookup table & shrink to 256 max
	{
		Soy::TScopeTimerPrint Timer("GifMakePalette",1);
		GifMakePalette( *pNewPaletteImage, Dither, pNewPalette );
	}
	
	Soy::Assert( pNewPalette != nullptr, "Failed to create palette");
	auto& NewPalette = *pNewPalette;
	
	std::shared_ptr<SoyPixels> pIndexedImage( new SoyPixels );
	auto& IndexedImage = *pIndexedImage;
	static bool DebugDrawPalette = false;
	
	
	if ( DebugDrawPalette )
	{
		Keyframe = true;
		IndexedImage.Init( width, height, SoyPixelsFormat::Greyscale );
		auto* PaletteRgb = &NewPalette.mPalette->GetPixelPtr(0,0,0);
		uint8 PaletteLocal[256*3];
		memcpy( PaletteLocal, PaletteRgb, 256*3 );
		for ( int y=0;	y<height;	y++ )
		{
			for ( int x=0;	x<width;	x++ )
			{
				IndexedImage.SetPixel( x, y, 0, y % NewPalette.GetSize() );
			}
		}
	}
	else if ( Dither )
	{
		Soy::TScopeTimerPrint Timer("GifDitherImage",1);
		SoyPixels ReducedImage;
		ReducedImage.Init( width, height, SoyPixelsFormat::RGBA );
		GifDitherImage( pPrevIndexedImage.get(), pPrevPalette.get(), image, ReducedImage, width, height, NewPalette);
		MakeIndexedImage( IndexedImage, ReducedImage );
	}
	else
	{
		//	slower ~200ms
		Soy::TScopeTimerPrint Timer("GifThresholdImage",1);
		IndexedImage.Init( width, height, SoyPixelsFormat::Greyscale );
		GifThresholdImage( pPrevIndexedImage.get(), pPrevPalette.get(), image, IndexedImage, width, height, NewPalette);
	}
	
	//	join together
	SoyPixelsFormat::MakePaletteised( PalettisedImage, IndexedImage, NewPalette.GetPalette(), NewPalette.GetTransparentIndex() );
}
	

TTextureBuffer::TTextureBuffer(std::shared_ptr<Opengl::TContext> OpenglContext) :
	mOpenglContext	( OpenglContext )
{
	
}

TTextureBuffer::~TTextureBuffer()
{
	Opengl::DeferDelete( mOpenglContext, mImage );
	mOpenglContext.reset();
}

