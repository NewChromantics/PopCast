#include "SoyGif.h"
#include "TFileCaster.h"

#include "gif.h"




//	watermark
//	https://www.shadertoy.com/view/ld33zX

namespace Gif
{
	void	MakeIndexedImage(SoyPixelsImpl& IndexedImage,const SoyPixelsImpl& Rgba);
}



const char* GifFragShaderSimpleNearest =
#include "GifNearest.frag"
;


const char* GifFragShaderDebugPalette =
#include "GifDebug.frag"
;



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
	Writer.Open = []{};
	Writer.fputc = Putc;
	Writer.fputs = Puts;
	Writer.fwrite = fwrite;
	Writer.Close = []{};
	

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
	Writer.Open = []{};
	Writer.fputc = Putc;
	Writer.fputs = Puts;
	Writer.fwrite = fwrite;

	auto PixelMeta = Streams[0].mPixelMeta;
	//	1/100ths of a sec *10 for ms.
	//	gr: add this to stream to estimates
	static uint16 Delay = 1;	//	Packet->mDuration
	auto Width = size_cast<uint16>( PixelMeta.GetWidth() );
	auto Height = size_cast<uint16>( PixelMeta.GetHeight() );
	GifBegin( Writer, Width, Height, Delay );

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
	//	https://bugzilla.mozilla.org/show_bug.cgi?id=232822
	static uint16 MinDurationMs = 1;
	auto delay = std::max( MinDurationMs, size_cast<uint16>( Packet->mDuration.GetTime() / 10 ) );
	

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
	//	stop thread
	SoyThread::Stop(false);
	
	//	if we're waiting on an opengl job, break it, as unity won't be running the opengl thread whilst destructing(running GC)
	auto OpenglSemaphore = mOpenglSemaphore;
	if ( OpenglSemaphore )
		OpenglSemaphore->OnFailed(__func__);
	
	//	wait for thread to finish
	WaitToFinish();
	
	//	deffered delete of stuff
	if ( mOpenglContext )
	{
		mPendingFramesLock.lock();
		mPendingFrames.Clear();
		mPendingFramesLock.unlock();
		
		Opengl::DeferDelete( mOpenglContext, mIndexImage );
		Opengl::DeferDelete( mOpenglContext, mPaletteImage );
		Opengl::DeferDelete( mOpenglContext, mSourceImage );
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
	Packet.mDuration = SoyTime( 16ull );
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
		
		mOpenglSemaphore.reset( new Soy::TSemaphore );
		if ( !IsWorking() )
			return true;

		//	copy for other thread in case the opengl job gets defferred for a long time and the buffer gets deleted in the meantime;
		//	run -> stop(in editor) -> opengl queue paused, frames deleted -> enable -> job runs -> frame is deleted
		std::shared_ptr<Soy::TSemaphore> SemaphoreCopy = mOpenglSemaphore;
		auto Read = [&Rgba,&Texture,SemaphoreCopy]
		{
			static bool Flip = false;
			//	gr: if semaphore has been aborted elsewhere (destructor) we know the Texture & Rgba are deleted!
			if ( SemaphoreCopy->IsCompleted() )
				return;
			Texture.mImage->Read( Rgba, SoyPixelsFormat::RGBA, Flip );
		};
		mOpenglContext->PushJob( Read, *mOpenglSemaphore );
		try
		{
			mOpenglSemaphore->Wait();
		}
		catch(std::exception& e)
		{
			SemaphoreCopy.reset();
			mOpenglSemaphore.reset();
			
			//	data semaphore may be early-aborted, but job still in queue (see above)
			std::Debug << "Failed to read pixels from pending gif-encoding buffer (dropping packet); " << e.what();
			//	drop packet
			return true;
		}
		mOpenglSemaphore.reset();
		Packet.mPixelBuffer.reset();
	}
	
	//	break out if the thread is stopped (gr: added here as we may break from the opengl job above for an early[ier] exit)
	if ( !IsWorking() )
		return true;
	
	//	make/find/copy palette
	//	reduce to 256
	//	mask
	//	push Palettised
	SoyPixels RgbaCopy;
	RgbaCopy.Copy( Rgba );
	SoyPixelsDef<Array<uint8>> PalettisedImage( Packet.mData, Packet.mMeta.mPixelMeta );
	
	try
	{
		static bool DebugPaletteShader = false;
		const char* Shader = GifFragShaderSimpleNearest;
		if ( DebugPaletteShader )
			Shader = GifFragShaderDebugPalette;
		
		MakePalettisedImage( PalettisedImage, RgbaCopy, Packet.mIsKeyFrame, Shader );
		Packet.mMeta.mCodec = SoyMediaFormat::FromPixelFormat( Packet.mMeta.mPixelMeta.GetFormat() );
	
		//	drop packets
		auto Block = []
		{
			return false;
		};
		
		TMediaEncoder::PushFrame( pPacket, Block );
	}
	catch(std::exception& e)
	{
		std::Debug << __func__ << " exception; " << e.what();
	}
	
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


void GifExtractPalette(const SoyPixelsImpl& Frame,SoyPixelsImpl& Palette,size_t PixelSkip)
{
	Soy::TScopeTimerPrint Timer( __func__, 1 );
	auto PixelStep = 1 + PixelSkip;
	auto PaletteSize = Frame.GetWidth() * Frame.GetHeight();
	Palette.Init( PaletteSize/PixelStep, 1, SoyPixelsFormat::RGB );
	
	if ( Frame.GetMeta() == Palette.GetMeta() )
	{
		Palette.GetPixelsArray().Copy( Frame.GetPixelsArray() );
	}
	else
	{
		for ( int i=0;	i<Palette.GetWidth();	i++ )
		{
			auto xy = Frame.GetXy( i*PixelStep );
			auto rgb = Frame.GetPixel3( xy.x, xy.y );
			Palette.SetPixel( i, 0, rgb );
		}
	}
}


void Gif::TEncoder::GetPalette(SoyPixelsImpl& Palette,const SoyPixelsImpl& Rgba,const SoyPixelsImpl* PrevPalette,const SoyPixelsImpl* PrevIndexedImage,TEncodeParams Params,bool& IsKeyframe)
{
	if ( PrevIndexedImage && !PrevPalette )
		throw Soy::AssertException("Cannot have a previous index image without a prev palette");

	if ( Params.mMakeDebugPalette )
	{
		IsKeyframe = true;
		GifDebugPalette( Palette );
		return;
	}

	if ( Params.mForcePrevPalette && PrevPalette )
	{
		IsKeyframe = false;
		Palette.Copy( *PrevPalette );
		return;
	}
	
	auto width = size_cast<uint32>(Rgba.GetWidth());
	auto height = size_cast<uint32>(Rgba.GetHeight());
	auto& RgbaArray = Rgba.GetPixelsArray();
	auto* RgbaData = RgbaArray.GetArray();
	auto* image = RgbaData;

	if ( Params.mAllowIntraFrames && PrevIndexedImage )
	{
		IsKeyframe = false;
		Soy::TScopeTimerPrint Timer("GifMakeDiffPalette",1);
		GifMakeDiffPalette( *PrevIndexedImage, *PrevPalette, image, width, height, Palette );

		//	todo: if we have all new palette (no transparencies) then set keyframe=true
		return;
	}

	IsKeyframe = true;
	GifExtractPalette( Rgba, Palette, Params.mFindPalettePixelSkip );
}

void Gif::TEncoder::ShrinkPalette(SoyPixelsImpl& Palette,bool Sort,size_t MaxPaletteSize,const TEncodeParams& Params)
{
	Soy::TScopeTimerPrint Timer( __func__, 1 );

	//	already shrunk
	if ( Palette.GetWidth() <= MaxPaletteSize && !Sort )
		return;
	
	std::shared_ptr<GifPalette> SmallPalette;
	GifMakePalette( Palette, false, SmallPalette, 256 );
	Palette.Copy( SmallPalette->GetPalette() );
	/*
	//	shrink to required size
	if ( Palette.GetWidth() > MaxPaletteSize )
		Palette.ResizeClip( MaxPaletteSize, 1 );
	 */
}

void Gif::TEncoder::IndexImageWithShader(SoyPixelsImpl& IndexedImage,const SoyPixelsImpl& Palette,const SoyPixelsImpl& Source,const char* FragShader)
{
	Soy::TScopeTimerPrint Timer( __func__, 1 );
	
	Soy::Assert( mOpenglContext!=nullptr, "Cannot use shader if no context");
	
	mOpenglSemaphore.reset( new Soy::TSemaphore );
	if ( !IsWorking() )
		return;
	std::shared_ptr<Soy::TSemaphore> SemaphoreCopy = mOpenglSemaphore;

	SoyPixelsMeta IndexMeta( Source.GetWidth(), Source.GetHeight(), SoyPixelsFormat::Greyscale );
	SoyPixelsMeta PaletteMeta( Palette.GetWidth(), Palette.GetHeight(), SoyPixelsFormat::RGB );
	
	auto Work = [this,SemaphoreCopy,&Source,&Palette,&IndexedImage,IndexMeta,PaletteMeta,FragShader]
	{
		if ( SemaphoreCopy->IsCompleted() )
			return;

		if ( !mIndexImage )
			mIndexImage.reset( new Opengl::TTexture( IndexMeta, GL_TEXTURE_2D ) );
		if ( !mPaletteImage )
			mPaletteImage.reset( new Opengl::TTexture( PaletteMeta, GL_TEXTURE_2D ) );
		if ( !mSourceImage )
			mSourceImage.reset( new Opengl::TTexture( Source.GetMeta(), GL_TEXTURE_2D ) );

		mSourceImage->Write( Source );
		mPaletteImage->Write( Palette );
		
		if ( !mOpenglBlitter )
			mOpenglBlitter.reset( new Opengl::TBlitter );
		
		Array<Opengl::TTexture> Sources;
		Sources.PushBack( *mSourceImage );
		Sources.PushBack( *mPaletteImage );
		
		Opengl::TTextureUploadParams UploadParams;
		{
			Soy::TScopeTimerPrint Timer( "Palettising blit", 2 );
			mOpenglBlitter->BlitTexture( *mIndexImage, GetArrayBridge(Sources), *mOpenglContext, UploadParams, FragShader );
		}
		mIndexImage->Read( IndexedImage );
	};
	
	mOpenglContext->PushJob( Work, *mOpenglSemaphore );
	try
	{
		mOpenglSemaphore->Wait();
	}
	catch(std::exception& e)
	{
		SemaphoreCopy.reset();
		mOpenglSemaphore.reset();
		
		throw;
	}
	mOpenglSemaphore.reset();
}


void Gif::TEncoder::MakePalettisedImage(SoyPixelsImpl& PalettisedImage,const SoyPixelsImpl& Rgba,bool& Keyframe,const char* IndexingShader)
{
	auto width = size_cast<uint32>(Rgba.GetWidth());
	auto height = size_cast<uint32>(Rgba.GetHeight());
	static bool Dither = false;
	
	auto& RgbaArray = PalettisedImage.GetPixelsArray();
	auto* RgbaData = RgbaArray.GetArray();
	auto* image = RgbaData;
	
	
	static TEncodeParams Params;
	
	auto pPrevIndexedImage = mPrevImageIndexes;
	auto pPrevPalette = mPrevPalette;

	if ( !Params.mAllowIntraFrames )
		pPrevIndexedImage.reset();
	
	std::shared_ptr<SoyPixelsImpl> pNewPalette;

	static int TransparentIndex = 0;
	//uint8 TransparentIndex = NewPalette.GetTransparentIndex();

	//	gr: this currently generates a full palette (can be > 256)
	bool IsPaletteKeyframe = true;
	pNewPalette.reset( new SoyPixels );
	GetPalette( *pNewPalette, Rgba, pPrevPalette.get(), pPrevIndexedImage.get(), Params, IsPaletteKeyframe );

	Keyframe = IsPaletteKeyframe;
	
	std::shared_ptr<SoyPixels> pIndexedImage( new SoyPixels );
	auto& IndexedImage = *pIndexedImage;
	
	if ( Params.mShaderPalettisiation )
	{
		//	gr: sort & shrink palette here, don't use lookup table
		ShrinkPalette( *pNewPalette, false, 255, Params );
		
		//	insert/override transparent
		//pNewPalette->SetPixel( TransparentIndex, 0, Rgb8(255,0,255) );
		
		IndexImageWithShader( IndexedImage, *pNewPalette, Rgba, IndexingShader );
	}
	else
	{
		std::shared_ptr<GifPalette> pNewGifPalette;
		//	make palette lookup table & shrink to 256 max
		{
			Soy::TScopeTimerPrint Timer("GifMakePalette",1);
			GifMakePalette( *pNewPalette, Dither, pNewGifPalette, 256 );
		}
		
		Soy::Assert( pNewGifPalette != nullptr, "Failed to create palette");
		auto& NewPalette = *pNewGifPalette;
		
		static bool DebugDrawPalette = true;
		
		
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
		
		pNewPalette->Copy( pNewGifPalette->GetPalette() );
	}
	
	//	join together
	Soy::TScopeTimerPrint Timer("MakePaletteised",1);
	SoyPixelsFormat::MakePaletteised( PalettisedImage, IndexedImage, *pNewPalette, TransparentIndex );
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

