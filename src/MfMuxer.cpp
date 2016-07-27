#include "MfMuxer.h"
#include "SoyMediaFoundation.h"

#include <mfapi.h>
#include <mfidl.h>
#include <Mfreadwrite.h>
#include <mferror.h>

#pragma comment(lib, "mfreadwrite")
#pragma comment(lib, "mfplat")
#pragma comment(lib, "mfuuid")



static auto BlitFragShader_RgbaToBgraAndFlip =
#include "BlitRgbaFlip.hlsl.frag"
;


TSinkWriter::TSinkWriter(const std::string& Filename)
{
	IMFAttributes* SinkAttribs = nullptr;
	IMFByteStream* OutputStream = nullptr;
	auto Filenamew = Soy::StringToWString( Filename );
	auto Result = MFCreateSinkWriterFromURL( Filenamew.c_str(), OutputStream, SinkAttribs, &mSinkWriter.mObject );
	MediaFoundation::IsOkay( Result, "MFCreateSinkWriterFromURL" );
	mSinkWriter->AddRef();
}

size_t TSinkWriter::CreateVideoStream(TMediaEncoderParams Params,SoyPixelsMeta InputMeta,size_t StreamIndex)
{
	//	restrictive sizes
	//	http://stackoverflow.com/questions/33030620/imfsinkwriter-cant-export-a-large-size-video-of-mp4
//	InputMeta.DumbSetWidth( 600 );
//	InputMeta.DumbSetHeight( 400 );

	auto OutputMeta = InputMeta;

	//	need to send frame rate/bitrate or we'll only see one frame
	TMediaEncoderParams InputParams;
	InputParams.mAverageBitRate = 0;
	InputParams.mFrameRate = 60;
	InputParams.mCodec = SoyMediaFormat::FromPixelFormat( InputMeta.GetFormat() );

	auto FormatOut = MediaFoundation::GetPlatformFormat( Params, OutputMeta.GetWidth(), OutputMeta.GetHeight() );
	auto FormatIn = MediaFoundation::GetPlatformFormat( InputParams, InputMeta.GetWidth(), InputMeta.GetHeight() );

	auto& SinkWriter = *mSinkWriter.mObject;
	
	//	assign output & input
	auto StreamIndexD = size_cast<DWORD>( StreamIndex );
	auto Result = SinkWriter.AddStream( FormatOut.mObject, &StreamIndexD );
	MediaFoundation::IsOkay( Result, "SinkWriter.AddStream" );
	IMFAttributes* EncodingAttribs = nullptr;
	Result = SinkWriter.SetInputMediaType( StreamIndex, FormatIn.mObject, EncodingAttribs );   
	MediaFoundation::IsOkay( Result, "SinkWriter.SetInputMediaType" );

	return StreamIndex;
}



MediaFoundation::TFileMuxer::TFileMuxer(const std::string& Filename,const TMediaEncoderParams& Params,std::shared_ptr<TMediaPacketBuffer>& Input,const std::function<void(bool&)>& OnStreamFinished) :
	TMediaMuxer				( nullptr, Input ),
	mMediaFoundationContext	( MediaFoundation::GetContext() ),
	mOnStreamFinished		( OnStreamFinished ),
	mStarted				( false ),
	mFinished				( false ),
	mEncoderParams			( Params )
{
	Soy::Assert( mMediaFoundationContext != nullptr, "Missing Media foundation context");
	mSinkWriter.reset( new TSinkWriter(Filename) );
}

size_t MediaFoundation::TFileMuxer::GetOutputStream(size_t InputStreamIndex)
{
	auto it = mInputToOutputStreamIndex.find( InputStreamIndex );
	if ( it == mInputToOutputStreamIndex.end() )
	{
		std::stringstream Error;
		Error << "No output stream for input stream " << InputStreamIndex;
		throw Soy::AssertException( Error.str() );
	}

	return it->second;
}

void MediaFoundation::TFileMuxer::SetupStreams(const ArrayBridge<TStreamMeta>&& Streams)
{
	Soy::Assert( mSinkWriter!=nullptr, "Missing sink writer");

	for ( int i=0;	i<Streams.GetSize();	i++ )
	{
		auto& Stream = Streams[i];

		//	make sure this stream doesnt already exist
		if ( mInputToOutputStreamIndex.find(Stream.mStreamIndex) != mInputToOutputStreamIndex.end() )
		{
			std::Debug << "Input stream " << Stream << " already has output stream (" << mInputToOutputStreamIndex[Stream.mStreamIndex] << ")" << std::endl;
			continue;
		}

		std::lock_guard<std::mutex> Lock( mWriteLock );
		try
		{
			auto OutputStreamIndex = mSinkWriter->CreateVideoStream( mEncoderParams, Stream.mPixelMeta, Stream.mStreamIndex );
			mInputToOutputStreamIndex[Stream.mStreamIndex] = OutputStreamIndex;
		}
		catch(std::exception& e)
		{
			std::Debug << "Failed to create output stream: " << Stream << ": " << e.what() << std::endl;
		}
	}
}



void MediaFoundation::TFileMuxer::ProcessPacket(std::shared_ptr<TMediaPacket> Packet,TStreamWriter& Output)
{
	Soy::Assert( Packet != nullptr, "Packet expected");
	std::lock_guard<std::mutex> Lock( mWriteLock );

	if ( !mStarted )
	{
		auto Result = mSinkWriter->mSinkWriter.mObject->BeginWriting();
		IsOkay( Result, "BeginWriting sink");
		mStarted = true;
	}

	if ( mFinished )
	{
		std::stringstream Error;
		Error << "Skipped writing frame " << Packet->mTimecode << "(dt=" << Packet->mTimecode << ") as we've already finished.";
		throw Soy::AssertException( Error.str() );
	}

	auto OutputStreamIndex = GetOutputStream( Packet->mMeta.mStreamIndex );
	auto Buffer = MediaFoundation::CreatePixelBuffer( *Packet );
	auto Result = mSinkWriter->mSinkWriter->WriteSample( size_cast<DWORD>( OutputStreamIndex ), Buffer.mObject );
	IsOkay( Result, "WriteSample sink");
}


void MediaFoundation::TFileMuxer::Finish()
{
	std::lock_guard<std::mutex> Lock( mWriteLock );
	if ( mStarted && !mFinished )
	{
		auto Result = mSinkWriter->mSinkWriter->Finalize();
		IsOkay( Result, "Finalize sink", false );
		mFinished = true;

		bool Dummy;
		mOnStreamFinished(Dummy);
	}
	else if ( !mStarted )
	{
		std::Debug << "SinkWriter not finished, never started." << std::endl;
	}
}



MfEncoder::MfEncoder(std::shared_ptr<TMediaPacketBuffer> OutputBuffer,size_t StreamIndex,SoyPixelsMeta OutputMeta) :
	TMediaEncoder	( OutputBuffer ),
	mOutputMeta		( OutputMeta ),
	mStreamIndex	( StreamIndex )
{
}


void MfEncoder::Write(const Opengl::TTexture& Image,SoyTime Timecode,Opengl::TContext& Context)
{
	throw Soy::AssertException("Not supported on this platform");
}



void MfEncoder::Write(const Directx::TTexture& Image,SoyTime Timecode,Directx::TContext& Context)
{
	//	blit texture
	auto* ContextCopy = &Context;
	auto CopyTexture = [this,Image,Timecode,ContextCopy]
	{
		//	alloc new texture from the pool
		auto& Pool = GetDirectxTexturePool();
		auto TextureMode = Directx::TTextureMode::ReadOnly;
		auto Alloc = [this,ContextCopy,TextureMode]
		{
			return std::make_shared<Directx::TTexture>( mOutputMeta, *ContextCopy, TextureMode );
		};
		auto Meta = Directx::TTextureMeta( mOutputMeta, TextureMode );
		auto CopyTarget = Pool.AllocPtr( Meta, Alloc );
		auto DeallocTemp = [&Pool,&CopyTarget]
		{
			if ( CopyTarget )
			{
				Pool.Release( CopyTarget );
				CopyTarget = nullptr;
			}
		};
			
		try
		{
			//	copy
			auto& Blitter = GetDirectxBlitter();

			BufferArray<Directx::TTexture,1> Sources;
			auto ImageMode = Image.GetMode();
			Sources.PushBack( Image );

			Blitter.BlitTexture( *CopyTarget, GetArrayBridge(Sources), *ContextCopy, "BlitFragShader_RgbaToBgraAndFlip", BlitFragShader_RgbaToBgraAndFlip );

			std::shared_ptr<TPixelBuffer> PixelBuffer;

			//	maybe can do this as a job later?
			static bool ReadPixelsNow = true;
			if ( ReadPixelsNow )
			{
				PixelBuffer.reset( new TDumbPixelBuffer() );
				auto& DumbPixelBuffer = static_cast<TDumbPixelBuffer&>( *PixelBuffer );
				CopyTarget->Read( DumbPixelBuffer.mPixels, *ContextCopy, Pool );
				Pool.Release( CopyTarget );
				CopyTarget = nullptr;
			}
			else
			{
				PixelBuffer.reset( new TTextureBuffer( CopyTarget, mDirectxTexturePool ) );

				//	don't release back to pool!
				CopyTarget = nullptr;
			}

			//	make output
			PushPixelBufferFrame( PixelBuffer, Timecode, mOutputMeta );
			DeallocTemp();
		}
		catch(...)
		{
			DeallocTemp();
			throw;
		}
	};

	Context.PushJob( CopyTexture );
}


void MfEncoder::Write(std::shared_ptr<SoyPixelsImpl> pImage,SoyTime Timecode)
{
	throw Soy::AssertException("Not supported on this platform");
}


void MfEncoder::OnError(const std::string& Error)
{
	mFatalError << Error;
}

void MfEncoder::PushPixelBufferFrame(std::shared_ptr<TPixelBuffer>& PixelBuffer,SoyTime Timecode,SoyPixelsMeta PixelMeta)
{
	Soy::Assert( PixelBuffer!=nullptr, "PixelBuffer expected");

	std::shared_ptr<TMediaPacket> pPacket( new TMediaPacket() );
	auto& Packet = *pPacket;

	Packet.mPixelBuffer = PixelBuffer;
	Packet.mTimecode = Timecode;
	Packet.mMeta.mCodec = SoyMediaFormat::FromPixelFormat( PixelMeta.GetFormat() );
	Packet.mMeta.mPixelMeta = PixelMeta;
	Packet.mMeta.mStreamIndex = mStreamIndex;

	auto Block = []()
	{
		return false;
	};

	PushFrame( pPacket, Block );
}

TPool<Directx::TTexture>& MfEncoder::GetDirectxTexturePool()
{
	if ( !mDirectxTexturePool )
	{
		static int Limit = 200;
		mDirectxTexturePool.reset( new TPool<Directx::TTexture>(Limit) );
	}
	return *mDirectxTexturePool;
}

Directx::TBlitter& MfEncoder::GetDirectxBlitter()
{
	if ( !mDirectxBlitter )
	{
		//alloc the pool
		GetDirectxTexturePool();
		mDirectxBlitter.reset( new Directx::TBlitter( mDirectxTexturePool ) );
	}
	return *mDirectxBlitter;
}

