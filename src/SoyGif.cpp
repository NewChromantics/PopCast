#include "SoyGif.h"
#include "TFileCaster.h"
#include "TBlitterOpengl.h"

#include "gif.h"




//	watermark
//	https://www.shadertoy.com/view/ld33zX

namespace Gif
{
	static uint64		TimerMinMs = 1000;
	
	void	MakeIndexedImage(SoyPixelsImpl& IndexedImage,const SoyPixelsImpl& Rgba);
	void	MaskImage(SoyPixelsImpl& RgbaMutable,const SoyPixelsImpl& PrevRgb,bool& Keyframe,bool TestAlpha,const TEncodeParams& Params,TMaskPixelFunc MaskPixelFunc);
	void	GetPalette(SoyPixelsImpl& Palette,const SoyPixelsImpl& Rgba,const TEncodeParams& Params,bool& IsKeyframe);
	void	ShrinkPalette(SoyPixelsImpl& Palette,bool Sort,const TEncodeParams& Params);
}



const char* GifFragShaderSimpleNearest =
#include "GifNearest.frag"
;


const char* GifFragShaderDebugPalette =
#include "GifDebug.frag"
;


std::shared_ptr<TMediaEncoder> Gif::AllocEncoder(std::shared_ptr<TMediaPacketBuffer> OutputBuffer,size_t StreamIndex,std::shared_ptr<Opengl::TContext> OpenglContext,const TEncodeParams& Params,bool SkipFrames)
{
	std::shared_ptr<TMediaEncoder> Encoder( new TEncoder( OutputBuffer, StreamIndex, OpenglContext, Params, SkipFrames ) );
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
	static uint16 LoopCount = 0;
	auto Width = size_cast<uint16>( PixelMeta.GetWidth() );
	auto Height = size_cast<uint16>( PixelMeta.GetHeight() );
	GifBegin( Writer, Width, Height, LoopCount );

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
	//	in chrome, 1(10ms) turns into 100ms
	static uint16 MinDurationMs = 2;
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
		
		size_t PaletteSize,TransparentIndex;
		SoyPixelsFormat::GetHeaderPalettised( GetArrayBridge( PalettisedImage.GetPixelsArray() ), PaletteSize, TransparentIndex );
		
		auto& IndexedImage = *PaletteAndIndexed[1];
		auto& Palette = *PaletteAndIndexed[0];
		
		//	fastish ~7ms
		Soy::TScopeTimerPrint Timer("GifWriteLzwImage", Gif::TimerMinMs );
		GifWriteLzwImage( LzwWriter, IndexedImage, 0, 0, delay, Palette, TransparentIndex );
		
		Output.Push( LzwWrite );
	}
	
	static bool DebugFin = false;
	if ( DebugFin )
		std::Debug << "GifWriteFrameFinished" << std::endl;
}


Opengl::GifBlitter::~GifBlitter()
{
	//	deffered delete of stuff
	DeferDelete( mContext, mIndexImage );
	DeferDelete( mContext, mPaletteImage );
	DeferDelete( mContext, mSourceImage );
	DeferDelete( mContext, mBlitter );
	
	mContext.reset();
}

Gif::TEncoder::TEncoder(std::shared_ptr<TMediaPacketBuffer>& OutputBuffer,size_t StreamIndex,std::shared_ptr<Opengl::TContext> Context,TEncodeParams Params,bool SkipFrames) :
	SoyWorkerThread	( "Gif::TEncoder", SoyWorkerWaitMode::Wake ),
	TMediaEncoder	( OutputBuffer ),
	mStreamIndex	( StreamIndex ),
	mOpenglGifBlitter	( new Opengl::GifBlitter(Context) ),
	mParams			( Params ),
	mPushedFrameCount	( 0 ),
	mSkipFrames		( SkipFrames )
{
	Start();
}

/*
Gif::TEncoder::TEncoder(std::shared_ptr<TMediaPacketBuffer>& OutputBuffer,size_t StreamIndex,std::shared_ptr<Directx::TContext> Context,TEncodeParams Params,bool SkipFrames) :
	SoyWorkerThread	( "Gif::TEncoder", SoyWorkerWaitMode::Wake ),
	TMediaEncoder	( OutputBuffer ),
	mStreamIndex	( StreamIndex ),
	mDirectxGifBlitter	( new Directx::GifBlitter(Context) ),
	mParams			( Params ),
	mPushedFrameCount	( 0 ),
	mSkipFrames		( SkipFrames )
{
	Start();
}
*/

Gif::TEncoder::~TEncoder()
{
	//	stop thread
	SoyThread::Stop(false);
	
	//	if we're waiting on an opengl job, break it, as unity won't be running the opengl thread whilst destructing(running GC)
	auto OpenglSemaphore = mDeviceJobSemaphore;
	if ( OpenglSemaphore )
		OpenglSemaphore->OnFailed(__func__);
	
	//	wait for thread to finish
	WaitToFinish();
	
	mPendingFramesLock.lock();
	mPendingFrames.Clear();
	mPendingFramesLock.unlock();

	mOpenglGifBlitter.reset();
//	mDirectxGifBlitter.reset();

}

void Gif::TEncoder::Write(const Opengl::TTexture& Image,SoyTime Timecode,Opengl::TContext& Context)
{
	//	all this needs to be done in the opengl thread
	auto MakePacketFromTexture = [this,Image,Timecode]
	{
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
	};
	
	auto& InternalContext = GetOpenglContext();
	InternalContext.PushJob( MakePacketFromTexture );
}


void Gif::TEncoder::Write(const Directx::TTexture& Image,SoyTime Timecode,Directx::TContext& Context)
{
	/*
	//	all this needs to be done in the opengl thread
	auto MakePacketFromTexture = [this,Image,Timecode]
	{
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
	};
	
	Context.PushJob( MakePacketFromTexture );
	*/
	throw Soy::AssertException("todo");
}

void Gif::TEncoder::Write(std::shared_ptr<SoyPixelsImpl> Image,SoyTime Timecode)
{
	Soy::Assert( Image != nullptr, "Gif::TEncoder::Write null pixels" );

	std::shared_ptr<TMediaPacket> pPacket( new TMediaPacket );
	auto& Packet = *pPacket;
	Packet.mPixelBuffer = std::make_shared<TTextureBuffer>( Image );
	Packet.mTimecode = Timecode;
	Packet.mDuration = SoyTime( 16ull );
	Packet.mMeta.mCodec = SoyMediaFormat::FromPixelFormat( Packet.mMeta.mPixelMeta.GetFormat() );
	PushFrame( pPacket );
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
	//	gr: pool this copy and write palettised right back to packet data
	std::shared_ptr<SoyPixelsImpl> Rgba;

	try
	{
		if ( Packet.mPixelBuffer )
		{
			auto& Texture = dynamic_cast<TTextureBuffer&>( *Packet.mPixelBuffer );
		
			mDeviceJobSemaphore.reset( new Soy::TSemaphore );
			if ( !IsWorking() )
				return true;

			//	make a copy for jobs
			std::shared_ptr<Soy::TSemaphore> SemaphoreCopy = mDeviceJobSemaphore;

			if ( mOpenglGifBlitter && Texture.mOpenglTexture )
			{
				//	copy for other thread in case the opengl job gets defferred for a long time and the buffer gets deleted in the meantime;
				//	run -> stop(in editor) -> opengl queue paused, frames deleted -> enable -> job runs -> frame is deleted
				auto Read = [&Rgba,&Texture,SemaphoreCopy]
				{
					static bool Flip = false;
					//	gr: if semaphore has been aborted elsewhere (destructor) we know the Texture & Rgba are deleted!
					if ( SemaphoreCopy->IsCompleted() )
						return;
					//SoyPixelsDef<Array<uint8>> Rgba( Packet.mData, Packet.mMeta.mPixelMeta );
					Rgba.reset( new SoyPixels );
					Texture.mOpenglTexture->Read( *Rgba, SoyPixelsFormat::RGBA, Flip );
				};
				auto& Context = GetOpenglContext();
				Context.PushJob( Read, *mDeviceJobSemaphore );
			}
			/*
			else if ( mDirextGifBlitter )
			{
				//	copy for other thread in case the opengl job gets defferred for a long time and the buffer gets deleted in the meantime;
				//	run -> stop(in editor) -> opengl queue paused, frames deleted -> enable -> job runs -> frame is deleted
				std::shared_ptr<Soy::TSemaphore> SemaphoreCopy = mDeviceJobSemaphore;
				auto Read = [&Rgba,&Texture,SemaphoreCopy]
				{
					static bool Flip = false;
					//	gr: if semaphore has been aborted elsewhere (destructor) we know the Texture & Rgba are deleted!
					if ( SemaphoreCopy->IsCompleted() )
						return;
					Texture.mOpenglTexture->Read( Rgba, SoyPixelsFormat::RGBA, Flip );
				};
				auto& Context = GetOpenglContext();
				GetOpenglContext().PushJob( Read, *mDeviceJobSemaphore );
			}
			*/
			else if ( Texture.mPixels )
			{
			}
			else
			{
				//	throw?
				mDeviceJobSemaphore->OnFailed("No graphics device blitter");
			}

			try
			{
				mDeviceJobSemaphore->Wait();
			}
			catch(...)
			{
				//	gr: needed?
				SemaphoreCopy.reset();
				throw;
			}
			mDeviceJobSemaphore.reset();
			Packet.mPixelBuffer.reset();
		}
	
		//	break out if the thread is stopped (gr: added here as we may break from the opengl job above for an early[ier] exit)
		if ( !IsWorking() )
			return true;
	
		Soy::Assert( Rgba != nullptr, "Came out of iteration with no rgba pixels of frame");
	}
	catch(std::exception& e)
	{
		//	gr: needed? always null here?
		mDeviceJobSemaphore.reset();
			
		//	data semaphore may be early-aborted, but job still in queue (see above)
		std::Debug << "Failed to read pixels from pending gif-encoding buffer (dropping packet); " << e.what();
		//	drop packet
		return true;
	}

	SoyPixelsDef<Array<uint8>> PalettisedImage( Packet.mData, Packet.mMeta.mPixelMeta );
	try
	{
		const char* Shader = GifFragShaderSimpleNearest;
		if ( mParams.mDebugIndexes )
			Shader = GifFragShaderDebugPalette;
		
		auto MaskPixel = [this](const vec3x<uint8>& Old,const vec3x<uint8>& New)
		{
			int Diff = abs( Old.x - New.x ) + abs( Old.y - New.y ) + abs( Old.z - New.z );
			float Difff = Diff / (256.f*3.f);
			if ( Difff > mParams.mMaskMaxDiff )
				return false;
			return true;
		};
		
		Soy::Assert( Rgba!=nullptr, "Rgba shouldnt be null here");
		if ( MakePalettisedImage( PalettisedImage, *Rgba, Packet.mIsKeyFrame, Shader, mParams, MaskPixel ) )
		{
			Packet.mMeta.mCodec = SoyMediaFormat::FromPixelFormat( Packet.mMeta.mPixelMeta.GetFormat() );
		
			//	drop packets
			auto Block = []
			{
				return false;
			};
			
			TMediaEncoder::PushFrame( pPacket, Block );
			mPushedFrameCount++;
		}
		else
		{
			//	all transparent, skipping packet. need to alter previous frame to extend time
			//	gr: with a special packet?
		}
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

	//	make this the latest frame in the queue
	if ( mSkipFrames )
		mPendingFrames.Clear();

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

std::shared_ptr<TTextureBuffer> Opengl::GifBlitter::CopyImmediate(const Opengl::TTexture& Image)
{
	std::shared_ptr<TTextureBuffer> Buffer( new TTextureBuffer(mContext) );
	Buffer->mOpenglTexture.reset( new Opengl::TTexture( Image.mMeta, GL_TEXTURE_2D ) );
	
	if ( !mBlitter )
		mBlitter.reset( new Opengl::TBlitter );

	BufferArray<Opengl::TTexture,1> ImageAsArray;
	ImageAsArray.PushBack( Image );
	Opengl::TTextureUploadParams Params;
	mBlitter->BlitTexture( *Buffer->mOpenglTexture, GetArrayBridge(ImageAsArray), *mContext, Params );

	return Buffer;
}




void GifExtractPalette(const SoyPixelsImpl& Frame,SoyPixelsImpl& Palette,size_t PixelSkip)
{
	Soy::TScopeTimerPrint Timer( __func__, Gif::TimerMinMs  );
	auto PixelStep = 1 + PixelSkip;
	auto PaletteSize = Frame.GetWidth() * Frame.GetHeight();
	Palette.Init( PaletteSize/PixelStep, 1, SoyPixelsFormat::RGB );
	
	//	gr: pre-determine if we have alphas
	bool HasAlphas = true;
	
	if ( Frame.GetMeta() == Palette.GetMeta() && !HasAlphas )
	{
		Palette.GetPixelsArray().Copy( Frame.GetPixelsArray() );
	}
	else
	{
		int PaletteIndex = 0;
		for ( int i=0;	i<Palette.GetWidth();	i++ )
		{
			auto xy = Frame.GetXy( i*PixelStep );
			auto rgba = Frame.GetPixel4( xy.x, xy.y );
			
			//	skip alphas
			if ( rgba.w == 0 )
				continue;
			
			Palette.SetPixel( PaletteIndex, 0, rgba.xyz() );
			PaletteIndex++;
		}
		
		//	clip unset
		//	gr: warning; growing here might insert odd colours
		if ( Palette.GetWidth() != PaletteIndex )
			Palette.ResizeClip( PaletteIndex, Palette.GetHeight() );
	}
}


void Gif::GetPalette(SoyPixelsImpl& Palette,const SoyPixelsImpl& Rgba,const TEncodeParams& Params,bool& IsKeyframe)
{
	if ( Params.mDebugPalette )
	{
		IsKeyframe = true;
		GifDebugPalette( Palette );
		return;
	}

	GifExtractPalette( Rgba, Palette, Params.mFindPalettePixelSkip );
}

void Gif::ShrinkPalette(SoyPixelsImpl& Palette,bool Sort,const TEncodeParams& Params)
{
	Soy::TScopeTimerPrint Timer( __func__, Gif::TimerMinMs );

	//	already shrunk
	if ( Palette.GetWidth() <= Params.mMaxColours && !Sort )
		return;
	
	std::shared_ptr<GifPalette> SmallPalette;
	GifMakePalette( Palette, false, SmallPalette, 256 );
	Palette.Copy( SmallPalette->GetPalette() );

	//	shrink to required size
	if ( Palette.GetWidth() > Params.mMaxColours )
		Palette.ResizeClip( Params.mMaxColours, 1 );
}



void Opengl::GifBlitter::IndexImageWithShader(SoyPixelsImpl& IndexedImage,const SoyPixelsImpl& Palette,const SoyPixelsImpl& Source,const char* FragShader,std::shared_ptr<Soy::TSemaphore>& JobSemaphore)
{
	Soy::Assert( JobSemaphore!=nullptr, "Job semaphore should already be allocated");
	Soy::Assert( mContext!=nullptr, "Cannot use shader if no context");
	
	std::shared_ptr<Soy::TSemaphore> SemaphoreCopy = JobSemaphore;

	SoyPixelsMeta IndexMeta( Source.GetWidth(), Source.GetHeight(), SoyPixelsFormat::Greyscale );
	SoyPixelsMeta PaletteMeta( Palette.GetWidth(), Palette.GetHeight(), SoyPixelsFormat::RGB );
	
	auto Work = [this,SemaphoreCopy,&Source,&Palette,&IndexedImage,IndexMeta,PaletteMeta,FragShader]
	{
		//	if this is already completed, then thread may be aborting and all this stuff may already be deleted
		if ( SemaphoreCopy->IsCompleted() )
			return;

		if ( !mIndexImage )
			mIndexImage.reset( new Opengl::TTexture( IndexMeta, GL_TEXTURE_2D ) );
		if ( !mPaletteImage )
			mPaletteImage.reset( new Opengl::TTexture( PaletteMeta, GL_TEXTURE_2D ) );
		if ( !mSourceImage )
			mSourceImage.reset( new Opengl::TTexture( Source.GetMeta(), GL_TEXTURE_2D ) );

		{
			Soy::TScopeTimerPrint Timer( "opengl: upload source images", Gif::TimerMinMs );
			mSourceImage->Write( Source );
			mPaletteImage->Write( Palette );
		}
		
		if ( !mBlitter )
			mBlitter.reset( new Opengl::TBlitter );
		
		Array<Opengl::TTexture> Sources;
		Sources.PushBack( *mSourceImage );
		Sources.PushBack( *mPaletteImage );
		
		Opengl::TTextureUploadParams UploadParams;
		{
			Soy::TScopeTimerPrint Timer( "Palettising blit", Gif::TimerMinMs );
			mBlitter->BlitTexture( *mIndexImage, GetArrayBridge(Sources), *mContext, UploadParams, FragShader );
		}
		{
			Soy::TScopeTimerPrint Timer( "opengl: read back indexed image", Gif::TimerMinMs );
			mIndexImage->Read( IndexedImage );
		}
	};
	
	mContext->PushJob( Work, *JobSemaphore );
	try
	{
		JobSemaphore->Wait();
	}
	catch(std::exception& e)
	{
		SemaphoreCopy.reset();
		JobSemaphore.reset();
		
		throw;
	}
	JobSemaphore.reset();
}


void Gif::TEncoder::IndexImageWithShader(SoyPixelsImpl& IndexedImage,const SoyPixelsImpl& Palette,const SoyPixelsImpl& Source,const char* FragShader)
{
	Soy::TScopeTimerPrint Timer( __func__, Gif::TimerMinMs );

	if ( !IsWorking() )
		return;
	mDeviceJobSemaphore.reset( new Soy::TSemaphore );
	if ( !IsWorking() )
		return;
	
	if ( mOpenglGifBlitter )
	{
		mOpenglGifBlitter->IndexImageWithShader( IndexedImage, Palette, Source, FragShader, mDeviceJobSemaphore );
		mDeviceJobSemaphore.reset();
	}
	else
	{
		mDeviceJobSemaphore.reset();
		//	todo: software fallback?
		throw Soy::AssertException("IndexImageWithShader no graphics context");
	}
}

void Gif::MaskImage(SoyPixelsImpl& RgbaMutable,const SoyPixelsImpl& PrevRgb,bool& Keyframe,bool TestAlpha,const TEncodeParams& Params,TMaskPixelFunc MaskPixelFunc)
{
	if ( TestAlpha )
	{
		//	mask image
		int HoleSize = 40;
		int Centerx = size_cast<int>(RgbaMutable.GetWidth()) / 2 - HoleSize;
		int Centery = size_cast<int>(RgbaMutable.GetHeight()) / 2 - HoleSize;
		for ( int y=-HoleSize;	y<HoleSize;	y++ )
		{
			for ( int x=-HoleSize;	x<HoleSize;	x++ )
			{
				RgbaMutable.SetPixel( Centerx+x, Centery+y, 3, 0 );
			}
		}
		Keyframe = false;
	}
	
	//	mask image
	if ( MaskPixelFunc )
	{
		for ( int y=0;	y<RgbaMutable.GetHeight();	y++ )
		{
			for ( int x=0;	x<RgbaMutable.GetWidth();	x++ )
			{
				auto* pOldRgba = &PrevRgb.GetPixelPtr( x, y, 0 );
				auto* pNewRgba = &RgbaMutable.GetPixelPtr( x, y, 0 );
				auto& OldRgba = *reinterpret_cast<const vec3x<uint8>*>(pOldRgba);
				auto& NewRgba = *reinterpret_cast<vec3x<uint8>*>(pNewRgba);
				if ( !MaskPixelFunc( OldRgba, NewRgba ) )
					continue;
				
				//	match, so alpha away
				pNewRgba[3] = 0;
				Keyframe = false;
			}
		}
	}
}

bool Gif::TEncoder::MakePalettisedImage(SoyPixelsImpl& PalettisedImage,const SoyPixelsImpl& Rgba,bool& Keyframe,const char* IndexingShader,const TEncodeParams& Params,TMaskPixelFunc MaskPixelFunc)
{
	std::shared_ptr<SoyPixelsImpl> pNewPalette;

	int TransparentIndex = 0;
	int DebugTransparentIndex = 1;
	static bool TestAlphaSquare = false;
	Keyframe = true;

	//	if frame count is too low, and 3 identical frames, we won't push the first X frames
	//	and from editor seems like it does nothing
	static int MinFramePushForIntra = 3;
	bool AllowIntraFrames = (mPushedFrameCount>MinFramePushForIntra) && Params.mAllowIntraFrames;
	bool DoMasking = TestAlphaSquare || AllowIntraFrames;
	
	std::shared_ptr<SoyPixelsImpl> RgbaCopy;
	if ( DoMasking && mPrevRgb )
	{
		Soy::Assert( Rgba.GetFormat() == SoyPixelsFormat::RGBA, "Need input to have an alpha channel. SHould be set form opengl read" );
		RgbaCopy.reset( new SoyPixels( Rgba ) );
		auto& RgbaMutable = const_cast<SoyPixelsImpl&>( Rgba );
		MaskImage( RgbaMutable, *mPrevRgb, Keyframe, TestAlphaSquare, Params, MaskPixelFunc );
	}
	

	//	gr: this currently generates a full palette (can be > 256)
	pNewPalette.reset( new SoyPixels );
	GetPalette( *pNewPalette, Rgba, Params, Keyframe );
	
	//	if the palette is empty... we're ALL transparent!
	if ( pNewPalette->GetWidth() == 0 )
		return false;

	std::shared_ptr<SoyPixels> pIndexedImage( new SoyPixels );
	auto& IndexedImage = *pIndexedImage;
	
	{
		//	gr: sort & shrink palette here, don't use lookup table
		ShrinkPalette( *pNewPalette, false, Params );
		
		//	insert/override transparent
		pNewPalette->SetPixel( TransparentIndex, 0, Params.mTransparentColour );
		
		IndexImageWithShader( IndexedImage, *pNewPalette, Rgba, IndexingShader );
	}
	
	//	save unmodifed frame
	//	gr: should this store(accumulate) a flattened image, so comparison with prev is palettised, rather than original
	if ( RgbaCopy )
		mPrevRgb = RgbaCopy;
	else
		mPrevRgb.reset( new SoyPixels( Rgba ) );
	
	//	join together
	Soy::TScopeTimerPrint Timer("MakePaletteised",Gif::TimerMinMs);
	auto WriteTransparentIndex = Params.mDebugTransparency ? DebugTransparentIndex : TransparentIndex;
	SoyPixelsFormat::MakePaletteised( PalettisedImage, IndexedImage, *pNewPalette, WriteTransparentIndex );
	
	return true;
}
	

TTextureBuffer::TTextureBuffer(std::shared_ptr<Opengl::TContext> OpenglContext) :
	mOpenglContext	( OpenglContext )
{
	
}

TTextureBuffer::~TTextureBuffer()
{
	if ( mOpenglContext )
	{
		Opengl::DeferDelete( mOpenglContext, mOpenglTexture );
		mOpenglContext.reset();
	}

	if ( mDirectxContext )
	{
		Opengl::DeferDelete( mDirectxContext, mDirectxTexture );
		mDirectxContext.reset();
	}

}

