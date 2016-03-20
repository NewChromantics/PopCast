#include "SoyGif.h"
#include "TFileCaster.h"
#include "TBlitterOpengl.h"
#include "TBlitterDirectx.h"
#include <SoyJson.h>

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



const char* GifFragShaderSimpleNearest_Opengl =
#include "GifNearest.glsl.frag"
;

const char* GifFragShaderDebugPalette_Opengl =
#include "GifDebug.glsl.frag"
;

const char* GifFragShaderSimpleNearest_Directx =
#include "GifNearest.hlsl.frag"
;

const char* GifFragShaderDebugPalette_Directx =
#include "GifDebug.hlsl.frag"
;


std::shared_ptr<TMediaEncoder> Gif::AllocEncoder(std::shared_ptr<TMediaPacketBuffer> OutputBuffer,size_t StreamIndex,std::shared_ptr<Opengl::TContext> Context,std::shared_ptr<TPool<Opengl::TTexture>> TexturePool,const TEncodeParams& Params,bool SkipFrames)
{
	std::shared_ptr<TMediaEncoder> Encoder( new TEncoder( OutputBuffer, StreamIndex, Context, TexturePool, Params, SkipFrames ) );
	return Encoder;
}

std::shared_ptr<TMediaEncoder> Gif::AllocEncoder(std::shared_ptr<TMediaPacketBuffer> OutputBuffer,size_t StreamIndex,std::shared_ptr<Directx::TContext> Context,std::shared_ptr<TPool<Directx::TTexture>> TexturePool,const TEncodeParams& Params,bool SkipFrames)
{
	std::shared_ptr<TMediaEncoder> Encoder( new TEncoder( OutputBuffer, StreamIndex, Context, TexturePool, Params, SkipFrames ) );
	return Encoder;
}


float GetColourScore(const vec3x<uint8>& a,const vec3x<uint8>& b)
{
	vec3x<int> Diff;
	Diff.x = abs(b.x - a.x);
	Diff.y = abs(b.y - a.y);
	Diff.z = abs(b.z - a.z);
	float Score = 1.0 - ( ( Diff.x + Diff.y + Diff.z) / 3.0f );
	return Score;
}


size_t FindNearestColour(const vec3x<uint8>& rgb,const SoyPixelsImpl& Palette)
{
	size_t Index = 0;
	float Score = 999.f;
	for ( int i = 0; i < Palette.GetWidth(); i++ )
	{
		auto& Pal = Palette.GetPixel3(0, 0);
		auto PalScore = GetColourScore(rgb, Pal);
		if ( PalScore > Score )
			continue;
		Score = PalScore;
		Index = i;
	}
	return Index;
}


std::shared_ptr<SoyPixelsImpl> TCpuGifBlitter::IndexImageWithShader(std::shared_ptr<SoyPixelsImpl> Palette,std::shared_ptr<SoyPixelsImpl> Source,const char* FragShader,std::shared_ptr<Soy::TSemaphore> JobSempahore)
{
	Soy::Assert(Palette != nullptr, "Expected palette");
	Soy::Assert(Source != nullptr, "Expected Source");
	Soy::Assert(JobSempahore != nullptr, "Expected Semaphore");

	std::shared_ptr<SoyPixelsImpl> pIndexedImage( new SoyPixels );
	auto& IndexedImage = *pIndexedImage;
	IndexedImage.Init( Source->GetWidth(), Source->GetHeight(), SoyPixelsFormat::Greyscale);

	//	brute force
	for ( int y = 0; y < IndexedImage.GetHeight(); y++ )
	{
		for ( int x = 0; x < IndexedImage.GetWidth(); x++ )
		{
			float xf = x / (float)IndexedImage.GetWidth();
			float yf = y / (float)IndexedImage.GetHeight();
			int sx = xf * Source->GetWidth();
			int sy = yf * Source->GetHeight();
			auto rgb = Source->GetPixel3(sx, sy);
			auto Index = FindNearestColour(rgb, *Palette);
			IndexedImage.SetPixel(x, y, 0, Index);
		}
	}

	JobSempahore->OnCompleted();
	return pIndexedImage;
}


Opengl::GifBlitter::GifBlitter(std::shared_ptr<TContext> Context,std::shared_ptr<TPool<TTexture>> TexturePool) :
	mContext		( Context ),
	mTexturePool	( TexturePool )
{
	Soy::Assert( mContext!=nullptr, "Opengl::GifBlitter Expected context");
	mBlitter.reset( new Opengl::TBlitter( mTexturePool ) );
}

Opengl::GifBlitter::~GifBlitter()
{
	//	deffered delete of stuff
	DeferDelete( mContext, mTexturePool );
	DeferDelete( mContext, mBlitter );
	
	mContext.reset();
}

std::shared_ptr<SoyPixelsImpl> Opengl::GifBlitter::IndexImageWithShader(std::shared_ptr<SoyPixelsImpl> Palette,std::shared_ptr<SoyPixelsImpl> Source,const char* FragShader,std::shared_ptr<Soy::TSemaphore> JobSemaphore)
{
	Soy::Assert(Palette != nullptr, "Expected palette");
	Soy::Assert(Source != nullptr, "Expected Source");
	Soy::Assert(JobSemaphore != nullptr, "Expected Semaphore");
	Soy::Assert( mContext!=nullptr, "Cannot use shader if no context");

	std::shared_ptr<SoyPixelsImpl> pIndexedImage( new SoyPixels );
	
	auto Work = [this,JobSemaphore,Source,Palette,pIndexedImage,FragShader]
	{
		SoyPixelsMeta IndexMeta( Source->GetWidth(), Source->GetHeight(), SoyPixelsFormat::Greyscale );

		auto Alloc = [IndexMeta]()
		{
			return std::make_shared<TTexture>( IndexMeta, GL_TEXTURE_2D );
		};

		//	pooled image
		auto IndexTexture = mTexturePool->Alloc( TTextureMeta(IndexMeta,GL_TEXTURE_2D), Alloc );
		
		Array<const SoyPixelsImpl*> Sources;
		Sources.PushBack( Source.get() );
		Sources.PushBack( Palette.get() );
		
		Opengl::TTextureUploadParams UploadParams;
		{
			Soy::TScopeTimerPrint Timer( "Palettising blit", Gif::TimerMinMs );
			mBlitter->BlitTexture( IndexTexture, GetArrayBridge(Sources), *mContext, UploadParams, FragShader );
		}
		{
			Soy::TScopeTimerPrint Timer( "read back indexed image", Gif::TimerMinMs );
			IndexTexture.Read( *pIndexedImage );
			mTexturePool->Release( IndexTexture );
		}
	};
	
	mContext->PushJob( Work, *JobSemaphore );
	JobSemaphore->Wait();
	return pIndexedImage;
}

std::shared_ptr<TTextureBuffer> Opengl::GifBlitter::CopyImmediate(const Opengl::TTexture& Image)
{
	std::shared_ptr<TTextureBuffer> Buffer( new TTextureBuffer(mContext) );
	Buffer->mOpenglTexture.reset( new TTexture( Image.mMeta, GL_TEXTURE_2D ) );
	
	if ( !mBlitter )
		mBlitter.reset( new TBlitter(nullptr) );

	BufferArray<TTexture,1> ImageAsArray;
	ImageAsArray.PushBack( Image );
	Opengl::TTextureUploadParams Params;
	mBlitter->BlitTexture( *Buffer->mOpenglTexture, GetArrayBridge(ImageAsArray), *mContext, Params );

	return Buffer;
}



Directx::GifBlitter::GifBlitter(std::shared_ptr<TContext> Context,std::shared_ptr<TPool<TTexture>> TexturePool) :
	mContext		( Context ),
	mTexturePool	( TexturePool )
{
	Soy::Assert( mContext!=nullptr, "GifBlitter Expected context");
	
	if ( !mTexturePool )
		mTexturePool.reset( new TPool<TTexture> );
		
	mBlitter.reset( new TBlitter(mTexturePool) );
}

Directx::GifBlitter::~GifBlitter()
{
	//	deffered delete of stuff
	Opengl::DeferDelete( mContext, mTexturePool );
	Opengl::DeferDelete( mContext, mBlitter );
	
	mContext.reset();
}

std::shared_ptr<SoyPixelsImpl> Directx::GifBlitter::IndexImageWithShader(std::shared_ptr<SoyPixelsImpl> Palette,std::shared_ptr<SoyPixelsImpl> Source,const char* FragShader,std::shared_ptr<Soy::TSemaphore> JobSemaphore)
{
	Soy::Assert(Palette != nullptr, "Expected palette");
	Soy::Assert(Source != nullptr, "Expected Source");
	Soy::Assert(JobSemaphore != nullptr, "Expected Semaphore");
	Soy::Assert( mContext!=nullptr, "Cannot use shader if no context");
	
	std::shared_ptr<SoyPixelsImpl> pIndexedImage( new SoyPixels );

	auto Work = [this,JobSemaphore,Source,Palette,pIndexedImage,FragShader]
	{
		//	job already aborted. likely: parent has been destructed, and dx hasn't updated until afterwards with this idle job
		//	ideally we'd have an extra lock and not allow access to this in case the destruction happens at the same time as the 
		//	dx job queue. "dont delete me" lock?
		if ( JobSemaphore->IsCompleted() )
			return;
		
		auto& IndexedImage = *pIndexedImage;
		SoyPixelsMeta IndexMeta( Source->GetWidth(), Source->GetHeight(), SoyPixelsFormat::Greyscale );

		Soy::TScopeTimerPrint Timer( "Directx thread IndexImageWithShader blit", 100 );

		auto& Context = *mContext;
		
		auto AllocTexture = [this,&IndexMeta]
		{
			return std::make_shared<TTexture>( IndexMeta, *mContext, TTextureMode::ReadOnly );
		};
		auto& IndexTexture = mTexturePool->Alloc( TTextureMeta(IndexMeta, TTextureMode::ReadOnly), AllocTexture );
		
		Array<const SoyPixelsImpl*> Sources;
		Sources.PushBack( Source.get() );
		Sources.PushBack( Palette.get() );
		
		Opengl::TTextureUploadParams UploadParams;
		{
			Soy::TScopeTimerPrint Timer( "Palettising blit", Gif::TimerMinMs );
			mBlitter->BlitTexture( IndexTexture, GetArrayBridge(Sources), Context, FragShader );	
		}
		{
			Soy::TScopeTimerPrint Timer( "read back indexed image", Gif::TimerMinMs );
			IndexTexture.Read( IndexedImage, Context );
			mTexturePool->Release( IndexTexture );
		}
	};
	
	mContext->PushJob( Work, *JobSemaphore );
	JobSemaphore->Wait();
	return pIndexedImage;
}

std::shared_ptr<TTextureBuffer> Directx::GifBlitter::CopyImmediate(const Directx::TTexture& Image)
{
	auto& Context = *mContext;
	std::shared_ptr<TTextureBuffer> Buffer( new TTextureBuffer(mContext) );
	auto& pTexture = Buffer->mDirectxTexture;
	pTexture.reset( new TTexture( Image.mMeta, *mContext, TTextureMode::ReadOnly ) );
	
	pTexture->Write( Image, Context );
	/*
	if ( !mBlitter )
		mBlitter.reset( new TBlitter );

	BufferArray<TTexture,1> ImageAsArray;
	ImageAsArray.PushBack( Image );
	Opengl::TTextureUploadParams Params;
	mBlitter->BlitTexture( *pTexture, GetArrayBridge(ImageAsArray), *mContext, Params );
	*/
	return Buffer;
}





Gif::TMuxer::TMuxer(std::shared_ptr<TStreamWriter>& Output,std::shared_ptr<TMediaPacketBuffer>& Input,const std::string& ThreadName,const TEncodeParams& Params) :
	TMediaMuxer		( Output, Input, std::string("Gif::TMuxer ")+ThreadName ),
	mFinished		( false ),
	mStarted		( false ),
	mLzwCompression	( Params.mLzwCompression )
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
	Soy::Assert( Packet->mMeta.mCodec == SoyMediaFormat::Palettised_RGB_8 || Packet->mMeta.mCodec == SoyMediaFormat::Palettised_RGBA_8, "Expected palettised image as codec");
	Soy::Assert( PalettisedImage.GetFormat() == SoyPixelsFormat::Palettised_RGB_8 || PalettisedImage.GetFormat() == SoyPixelsFormat::Palettised_RGBA_8, "Expected palettised image in pixel meta");
	
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
		GifWriteLzwImage( LzwWriter, IndexedImage, 0, 0, delay, Palette, TransparentIndex, mLzwCompression );
		
		Output.Push( LzwWrite );
	}
	
	static bool DebugFin = false;
	if ( DebugFin )
		std::Debug << "GifWriteFrameFinished" << std::endl;
}


Gif::TEncoder::TEncoder(std::shared_ptr<TMediaPacketBuffer>& OutputBuffer,size_t StreamIndex,std::shared_ptr<Opengl::TContext> Context,std::shared_ptr<TPool<Opengl::TTexture>> TexturePool,TEncodeParams Params,bool SkipFrames) :
	SoyWorkerThread		( "Gif::TEncoder", SoyWorkerWaitMode::Wake ),
	TMediaEncoder		( OutputBuffer ),
	mStreamIndex		( StreamIndex ),
	mOpenglGifBlitter	( new Opengl::GifBlitter(Context,TexturePool) ),
	mParams				( Params ),
	mPushedFrameCount	( 0 ),
	mSkipFrames			( SkipFrames )
{
	Start();
}


Gif::TEncoder::TEncoder(std::shared_ptr<TMediaPacketBuffer>& OutputBuffer,size_t StreamIndex,std::shared_ptr<Directx::TContext> Context,std::shared_ptr<TPool<Directx::TTexture>> TexturePool,TEncodeParams Params,bool SkipFrames) :
	SoyWorkerThread		( "Gif::TEncoder", SoyWorkerWaitMode::Wake ),
	TMediaEncoder		( OutputBuffer ),
	mStreamIndex		( StreamIndex ),
	mDirectxGifBlitter	( new Directx::GifBlitter(Context,TexturePool) ),
	mParams				( Params ),
	mPushedFrameCount	( 0 ),
	mSkipFrames			( SkipFrames )
{
	Start();
}


Gif::TEncoder::TEncoder(std::shared_ptr<TMediaPacketBuffer>& OutputBuffer,size_t StreamIndex,TEncodeParams Params,bool SkipFrames) :
	SoyWorkerThread		( "Gif::TEncoder", SoyWorkerWaitMode::Wake ),
	TMediaEncoder		( OutputBuffer ),
	mStreamIndex		( StreamIndex ),
	mCpuGifBlitter		( new TCpuGifBlitter() ),
	mParams				( Params ),
	mPushedFrameCount	( 0 ),
	mSkipFrames			( SkipFrames )
{
	Start();
}


Gif::TEncoder::~TEncoder()
{
	//	stop thread
	SoyThread::Stop(false);
	
	//	if we're waiting on an opengl job, break it, as unity won't be running the opengl thread whilst destructing(running GC)
	AbortJobSemaphores();
	
	//	wait for thread to finish
	WaitToFinish();
	
	mPendingFramesLock.lock();
	mPendingFrames.Clear();
	mPendingFramesLock.unlock();

	mOpenglGifBlitter.reset();
//	mDirectxGifBlitter.reset();

}


std::shared_ptr<Soy::TSemaphore> Gif::TEncoder::AllocJobSempahore()
{
	std::lock_guard<std::mutex> Lock( mJobSemaphoresLock );

	auto Semaphore = std::make_shared<Soy::TSemaphore>();
	mJobSemaphores.PushBack( Semaphore );
	return Semaphore;
}

void Gif::TEncoder::AbortJobSemaphores()
{
	//	pop each one, abort it and free
	while ( true )
	{
		std::lock_guard<std::mutex> Lock( mJobSemaphoresLock );
		if ( mJobSemaphores.IsEmpty() )
			break;
		auto Semaphore = mJobSemaphores.PopAt(0);
		Semaphore->OnFailed(__func__);
		Semaphore.reset();
	}
}



void Gif::TEncoder::Write(const Opengl::TTexture& Image,SoyTime Timecode,Opengl::TContext& Context)
{
	if ( mParams.mCpuOnly )
		throw Soy::AssertException("GPU mode disabled");

	//	all this needs to be done in the opengl thread
	auto MakePacketFromTexture = [this,Image,Timecode]
	{
		std::shared_ptr<TMediaPacket> pPacket( new TMediaPacket );
		auto& Packet = *pPacket;
		Packet.mPixelBuffer = mOpenglGifBlitter->CopyImmediate( Image );
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
	if ( mParams.mCpuOnly )
		throw Soy::AssertException("GPU mode disabled");

	//	pre-emptively skip frames
	if ( !CanPushFrame(Timecode) )
	{
		OnFramePrePushSkipped(Timecode);
		return;
	}

	//	hacky! but the job doesn't have access to its own context...
	Directx::TContext* ContextCopy = &Context;

	//	does all this needs to be done in the dx thread?
	auto MakePacketFromTexture = [this,Image,Timecode,ContextCopy]
	{
		std::shared_ptr<TMediaPacket> pPacket( new TMediaPacket );
		auto& Packet = *pPacket;
		Packet.mPixelBuffer = mDirectxGifBlitter->CopyImmediate( Image );
		Packet.mTimecode = Timecode;
		Packet.mDuration = SoyTime( 16ull );
		Packet.mMeta.mCodec = SoyMediaFormat::FromPixelFormat( Packet.mMeta.mPixelMeta.GetFormat() );
		
		PushFrame( pPacket );
	};
	
	auto& InternalContext = GetDirectxContext();
	InternalContext.PushJob( MakePacketFromTexture );
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
		
			if ( !IsWorking() )
				return true;


			if ( mOpenglGifBlitter && Texture.mOpenglTexture )
			{
				auto Semaphore = AllocJobSempahore();
				//	copy for other thread in case the opengl job gets defferred for a long time and the buffer gets deleted in the meantime;
				//	run -> stop(in editor) -> opengl queue paused, frames deleted -> enable -> job runs -> frame is deleted
				auto Read = [&Rgba,Packet,Semaphore]
				{
					static bool Flip = false;
					//	gr: if semaphore has been aborted elsewhere (destructor) we know the Texture & Rgba are deleted!
					if ( Semaphore->IsCompleted() )
						return;

					Rgba.reset( new SoyPixels );
					auto& Texture = dynamic_cast<TTextureBuffer&>( *Packet.mPixelBuffer );
					Texture.mOpenglTexture->Read( *Rgba, SoyPixelsFormat::RGBA, Flip );
				};
				auto& Context = GetOpenglContext();
				Context.PushJob( Read, *Semaphore );
				Semaphore->Wait();
			}
			else if ( mDirectxGifBlitter && Texture.mDirectxTexture )
			{
				auto Semaphore = AllocJobSempahore();
				//	copy for other thread in case the opengl job gets defferred for a long time and the buffer gets deleted in the meantime;
				//	run -> stop(in editor) -> opengl queue paused, frames deleted -> enable -> job runs -> frame is deleted
				auto Read = [&Rgba,Packet,Semaphore,this]
				{
					static bool Flip = false;
					//	gr: if semaphore has been aborted elsewhere (destructor) we know the Texture & Rgba are deleted!
					if ( Semaphore->IsCompleted() )
						return;

					Rgba.reset( new SoyPixels );
					auto& Texture = dynamic_cast<TTextureBuffer&>( *Packet.mPixelBuffer );
					Texture.mDirectxTexture->Read( *Rgba, *mDirectxGifBlitter->mContext );
				};
				auto& Context = GetDirectxContext();
				Context.PushJob( Read, *Semaphore );
				Semaphore->Wait();
			}
			else if ( Texture.mPixels )
			{
				Rgba = Texture.mPixels;
			}
			else
			{
				//	throw?
				throw Soy::AssertException("No graphics device blitter");
			}
			Packet.mPixelBuffer.reset();
		}
	
		//	break out if the thread is stopped (gr: added here as we may break from the opengl job above for an early[ier] exit)
		if ( !IsWorking() )
			return true;
	
		Soy::Assert( Rgba != nullptr, "Came out of iteration with no rgba pixels of frame");
	}
	catch(std::exception& e)
	{
		//	data semaphore may be early-aborted, but job still in queue (see above)
		std::Debug << "Failed to read pixels from pending gif-encoding buffer (dropping packet); " << e.what();
		//	drop packet
		return true;
	}

	SoyPixelsDef<Array<uint8>> PalettisedImage( Packet.mData, Packet.mMeta.mPixelMeta );
	try
	{
		const char* Shader = nullptr;

		if ( mOpenglGifBlitter && !mParams.mDebugIndexes )
		{
			Shader = GifFragShaderSimpleNearest_Opengl;
		}
		else if ( mOpenglGifBlitter && mParams.mDebugIndexes )
		{
			Shader = GifFragShaderDebugPalette_Opengl;
		}
		else if ( mDirectxGifBlitter && !mParams.mDebugIndexes )
		{
			Shader = GifFragShaderSimpleNearest_Directx;
		}
		else if ( mDirectxGifBlitter && mParams.mDebugIndexes )
		{
			Shader = GifFragShaderDebugPalette_Directx;
		}

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

Opengl::TContext& Gif::TEncoder::GetOpenglContext()
{
	auto& Blitter = mOpenglGifBlitter;
	Soy::Assert( Blitter != nullptr, std::string(__func__) + " missing opengl gif blitter");
	auto Context = Blitter->mContext;
	Soy::Assert( Context!= nullptr, std::string(__func__) + " missing opengl gif blitter context");
	return *Context;
}

Directx::TContext& Gif::TEncoder::GetDirectxContext()
{
	auto& Blitter = mDirectxGifBlitter;
	Soy::Assert( Blitter != nullptr, std::string(__func__) + " missing directx gif blitter");
	auto Context = Blitter->mContext;
	Soy::Assert( Context!= nullptr, std::string(__func__) + " missing directx gif blitter context");
	return *Context;
}

void Gif::TEncoder::GetMeta(TJsonWriter& Json)
{
	auto PendingFrameCount = mPendingFrames.GetSize();

	Json.Push("PushedFrameCount", mPushedFrameCount);
	Json.Push("PendingFrameCount", PendingFrameCount);
}


void GifExtractPalette(const SoyPixelsImpl& Frame,SoyPixelsImpl& Palette,size_t PixelSkip)
{
	Soy::Assert( Frame.GetFormat() == SoyPixelsFormat::RGBA, "GifExtractPalette currently requires RGBA" );

	Soy::TScopeTimerPrint Timer( __func__, Gif::TimerMinMs  );
	auto PixelStep = 1 + PixelSkip;
	auto PaletteSize = Frame.GetWidth() * Frame.GetHeight();
	//	dx requires RGBA, not RGB
	Palette.Init( PaletteSize/PixelStep, 1, SoyPixelsFormat::RGBA );
	
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


std::shared_ptr<SoyPixelsImpl> Gif::TEncoder::IndexImageWithShader(std::shared_ptr<SoyPixelsImpl> Palette, std::shared_ptr<SoyPixelsImpl> Source, const char* FragShader)
{
	Soy::TScopeTimerPrint Timer( __func__, Gif::TimerMinMs );

	if ( !IsWorking() )
		return nullptr;
	
	auto Semaphore = AllocJobSempahore();

	if ( mOpenglGifBlitter && !mParams.mCpuOnly )
	{
		return mOpenglGifBlitter->IndexImageWithShader( Palette, Source, FragShader, Semaphore );
	}
	else if ( mDirectxGifBlitter && !mParams.mCpuOnly )
	{
		return mDirectxGifBlitter->IndexImageWithShader( Palette, Source, FragShader, Semaphore );
	}
	else
	{
		return mCpuGifBlitter->IndexImageWithShader( Palette, Source, FragShader, Semaphore );
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

bool Gif::TEncoder::MakePalettisedImage(SoyPixelsImpl& PalettisedImage,const SoyPixelsImpl& OriginalRgba,bool& Keyframe,const char* IndexingShader,const TEncodeParams& Params,TMaskPixelFunc MaskPixelFunc)
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
	
	std::shared_ptr<SoyPixelsImpl> Rgba( new SoyPixels( OriginalRgba ) );

	if ( DoMasking && mPrevRgb )
	{
		Soy::Assert( Rgba->GetFormat() == SoyPixelsFormat::RGBA, "Need input to have an alpha channel. SHould be set form opengl read" );
		auto& RgbaMutable = const_cast<SoyPixelsImpl&>( *Rgba );
		MaskImage( RgbaMutable, *mPrevRgb, Keyframe, TestAlphaSquare, Params, MaskPixelFunc );
	}
	
	//	gr: this currently generates a full palette (can be > 256)
	pNewPalette.reset( new SoyPixels );
	GetPalette( *pNewPalette, *Rgba, Params, Keyframe );
	
	//	if the palette is empty... we're ALL transparent!
	if ( pNewPalette->GetWidth() == 0 )
		return false;

	//	gr: sort & shrink palette here, don't use lookup table
	ShrinkPalette( *pNewPalette, false, Params );
		
	//	insert/override transparent
	pNewPalette->SetPixel( TransparentIndex, 0, Params.mTransparentColour );
		
	std::shared_ptr<SoyPixelsImpl> pIndexedImage = IndexImageWithShader( pNewPalette, Rgba, IndexingShader );
	Soy::Assert(pIndexedImage != nullptr, "Failed to make indexed image");

	//	save unmodifed frame
	//	gr: should this store(accumulate) a flattened image, so comparison with prev is palettised, rather than original
	mPrevRgb = Rgba;
	
	//	join together
	Soy::TScopeTimerPrint Timer("MakePaletteised",Gif::TimerMinMs);
	auto WriteTransparentIndex = Params.mDebugTransparency ? DebugTransparentIndex : TransparentIndex;
	SoyPixelsFormat::MakePaletteised( PalettisedImage, *pIndexedImage, *pNewPalette, WriteTransparentIndex );
	
	return true;
}

bool Gif::TEncoder::CanPushFrame(SoyTime Timecode)
{
	//	if skipping and we have something pending, skip this one
	if ( mSkipFrames )
	{
		static int MaxPending = 1;
		auto PendingCount = mPendingFrames.GetSize();
		if ( PendingCount >= MaxPending )
			return false;
	}

	//	pool is full (and other OOM checks)
	if ( mDirectxGifBlitter )
	{
		if ( mDirectxGifBlitter->GetTexturePool().IsFull() )
			return false;
	}

	if ( mOpenglGifBlitter )
	{
		if ( mOpenglGifBlitter->GetTexturePool().IsFull() )
			return false;
	}

	return true;
}

void Gif::TEncoder::OnFramePrePushSkipped(SoyTime Timecode)
{
	mOnFramePushSkipped.OnTriggered(Timecode);
}


TTextureBuffer::TTextureBuffer(std::shared_ptr<Opengl::TContext> Context) :
	mOpenglContext	( Context )
{	
}

TTextureBuffer::TTextureBuffer(std::shared_ptr<Directx::TContext> Context) :
	mDirectxContext	( Context )
{
}

TTextureBuffer::TTextureBuffer(std::shared_ptr<SoyPixelsImpl> Pixels) :
	mPixels	( Pixels )
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

