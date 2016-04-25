#include "MfMuxer.h"
#include "SoyMediaFoundation.h"

#include <mfapi.h>
#include <mfidl.h>
#include <Mfreadwrite.h>
#include <mferror.h>

#pragma comment(lib, "mfreadwrite")
#pragma comment(lib, "mfplat")
#pragma comment(lib, "mfuuid")


const UINT32 VIDEO_WIDTH = 640;
const UINT32 VIDEO_HEIGHT = 480;
const UINT32 VIDEO_FPS = 30;
const UINT64 VIDEO_FRAME_DURATION = 10 * 1000 * 1000 / VIDEO_FPS;
const UINT32 VIDEO_BIT_RATE = 800000;
const GUID   VIDEO_ENCODING_FORMAT = MFVideoFormat_WMV3;
const GUID   VIDEO_INPUT_FORMAT = MFVideoFormat_RGB32;
const UINT32 VIDEO_PELS = VIDEO_WIDTH * VIDEO_HEIGHT;
const UINT32 VIDEO_FRAME_COUNT = 20 * VIDEO_FPS;




TSinkWriter::TSinkWriter(const std::string& Filename)
{
	IMFAttributes* SinkAttribs = nullptr;
	IMFByteStream* OutputStream = nullptr;
	auto Filenamew = Soy::StringToWString( Filename );
	auto Result = MFCreateSinkWriterFromURL( Filenamew.c_str(), OutputStream, SinkAttribs, &mSinkWriter.mObject );
	MediaFoundation::IsOkay( Result, "MFCreateSinkWriterFromURL" );
	mSinkWriter->AddRef();
}

size_t TSinkWriter::CreateVideoStream(SoyMediaFormat::Type Format,size_t FrameRate,size_t BitRate,SoyPixelsMeta InputMeta)
{
	auto OutputMeta = InputMeta;
	
	auto FormatOut = MediaFoundation::CreateFormat( SoyMediaFormat::H264_ES, FrameRate, BitRate, OutputMeta.GetWidth(), OutputMeta.GetHeight() );
	auto FormatIn = MediaFoundation::CreateFormat( SoyMediaFormat::FromPixelFormat( InputMeta.GetFormat() ), FrameRate, BitRate, InputMeta.GetWidth(), InputMeta.GetHeight() );

	auto& SinkWriter = *mSinkWriter.mObject;
	
	//	assign output & input
	DWORD StreamIndex = 0;
	auto Result = SinkWriter.AddStream( FormatOut.mObject, &StreamIndex );
	MediaFoundation::IsOkay( Result, "SinkWriter.AddStream" );
	IMFAttributes* EncodingAttribs = nullptr;
	Result = SinkWriter.SetInputMediaType( StreamIndex, FormatIn.mObject, EncodingAttribs );   
	MediaFoundation::IsOkay( Result, "SinkWriter.SetInputMediaType" );


	//	setup all streams, start
	//	gr: deffer this until after we have the first frame?
	Result = SinkWriter.BeginWriting();
	MediaFoundation::IsOkay( Result, "SinkWriter.BeginWriting" );

	return StreamIndex;
}



MediaFoundation::TFileMuxer::TFileMuxer(const std::string& Filename,std::shared_ptr<TMediaPacketBuffer>& Input,const std::function<void(bool&)>& OnStreamFinished) :
	TMediaMuxer			( nullptr, Input ),
	mOnStreamFinished	( OnStreamFinished ),
	mStarted			( false ),
	mFinished			( false )
{
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
			auto FrameRate = size_cast<size_t>( Stream.mFramesPerSecond );
			if ( FrameRate == 0 )
				FrameRate = 60;
			auto BitRate = Stream.mAverageBitRate;
			if ( BitRate == 0 )
				BitRate = 8000;

			auto OutputFormat = SoyMediaFormat::H264_ES;
			auto OutputStreamIndex = mSinkWriter->CreateVideoStream( OutputFormat, FrameRate, BitRate, Stream.mPixelMeta );
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

	auto OutputStreamIndex = GetOutputStream( Packet->mMeta.mStreamIndex );
	auto Buffer = MediaFoundation::CreatePixelBuffer( *Packet );
	auto Result = mSinkWriter->mSinkWriter->WriteSample( size_cast<DWORD>( OutputStreamIndex ), Buffer.mObject );
	IsOkay( Result, "WriteSample sink");
}


void MediaFoundation::TFileMuxer::Finish()
{
	std::lock_guard<std::mutex> Lock( mWriteLock );
	if ( !mFinished )
	{
		auto Result = mSinkWriter->mSinkWriter->Finalize();
		IsOkay( Result, "Finalize sink");
		mFinished = true;
	}
}
