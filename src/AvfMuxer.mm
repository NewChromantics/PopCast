#include "AvfMuxer.h"
#include "SoyAvf.h"

class Avf::TAssetWriterInput
{
public:
	TAssetWriterInput(const TStreamMeta& Stream,TAssetWriter& Parent);
	
	void							Finish();
	
public:
	std::atomic<bool>				mFinished;
	std::mutex						mLock;		//	to coordinate cleanup
	ObjcPtr<AVAssetWriterInput>		mInput;
};

class Avf::TAssetWriter
{
public:
	TAssetWriter(const std::string& Filename);
	
public:
	ObjcPtr<AVAssetWriter>		mAsset;
};



Avf::TAssetWriter::TAssetWriter(const std::string& Filename)
{
	Array<std::string> FilenameParts;
	Soy::StringSplitByString( GetArrayBridge(FilenameParts), Filename, "." );
	if ( FilenameParts.GetSize() < 2 )
	{
		std::stringstream Error;
		Error << "Failed to get extension from fileanme " << Filename;
		throw Soy::AssertException( Error.str() );
	}
	
	auto& FilenameExtension = FilenameParts.GetBack();
	auto* FileType = Avf::GetFileExtensionType( FilenameExtension );
	//	auto* Url = Avf::GetUrl( Filename );
	
	auto FilenameNs = Soy::StringToNSString( Filename );
	//auto* Url = [NSURL fileURLWithPath:FilenameNs];
	auto* Url = [NSURL fileURLWithPath:FilenameNs isDirectory:false];
	
	NSError* AssetError = nil;
	mAsset.mObject = [[AVAssetWriter alloc] initWithURL:Url fileType:FileType error:&AssetError];
	if ( !mAsset )
	{
		std::stringstream Error;
		Error << "Failed to create asset writer; " << Soy::NSErrorToString(AssetError);
		throw Soy::AssertException( Error.str() );
	}
	
	//	delete file
	//	delete file before starting or startWriting will error
	unlink( Filename.c_str() );

}


Avf::TAssetWriterInput::TAssetWriterInput(const TStreamMeta& Stream,TAssetWriter& Parent) :
	mFinished	( false )
{
	//auto* Type = Avf::GetFormatType( Stream.mCodec );
	auto* Writer = Parent.mAsset.mObject;
	
	//	http://stackoverflow.com/questions/13138385/how-to-use-avassetwriter-to-write-h264-strem-into-video
	/*
	NSDictionary *videoSettings = [NSDictionary dictionaryWithObjectsAndKeys:
								   AVVideoCodecH264, AVVideoCodecKey,
								   [NSNumber numberWithInt:192], AVVideoWidthKey,
								   [NSNumber numberWithInt:144], AVVideoHeightKey,
								   videoCompressionProps, AVVideoCompressionPropertiesKey,
								   nil];
	 sourceFormatHint:myVideoFormat
	 CMFormatDescriptionRef videoFormat=NULL;
	*/
	bool SourceCompressed = Avf::IsFormatCompressed( Stream.mCodec );
	if ( SourceCompressed )
	{
		auto Format = GetFormatDescription( Stream );
		mInput.Retain( [AVAssetWriterInput assetWriterInputWithMediaType:AVMediaTypeVideo outputSettings:nil sourceFormatHint:Format] );
	}
	else
	{
		//	gr: I cannot make this work with nil for output settings :/
		//	re: nil;
		//	https://developer.apple.com/library/ios/documentation/AudioVideo/Conceptual/AVFoundationPG/Articles/05_Export.html
		//	 If you want the media data to be written in the format in which it was stored, pass nil in the outputSettings parameter.
		//	Pass nil only if the asset writer was initialized with a fileType of AVFileTypeQuickTimeMovie.
		
		auto Width = size_cast<int>(Stream.mPixelMeta.GetWidth());
		auto Height = size_cast<int>(Stream.mPixelMeta.GetHeight());
		NSDictionary* videoSettings = [NSDictionary dictionaryWithObjectsAndKeys:
									   AVVideoCodecH264, AVVideoCodecKey,
									   [NSNumber numberWithInt:Width], AVVideoWidthKey,
									   [NSNumber numberWithInt:Height], AVVideoHeightKey,
									   //videoCompressionProps, AVVideoCompressionPropertiesKey,
									   nil];
		mInput.Retain( [AVAssetWriterInput assetWriterInputWithMediaType:AVMediaTypeVideo outputSettings:videoSettings] );
	}
	
	if ( !mInput )
	{
		std::stringstream Error;
		Error << "Failed to create asset writer input; ";
		throw Soy::AssertException( Error.str() );
	}
	
	//	for interleaved data, use NO
	//mInput.mObject.expectsMediaDataInRealTime = YES;
	
	//	set other meta
	//assetWriterInput.transform = videoAssetTrack.preferredTransform;
	
	if ( ![Writer canAddInput:mInput.mObject] )
	{
		std::stringstream Error;
		Error << "Failed to create input writer compatible with writer; ";
		throw Soy::AssertException( Error.str() );
	}

	
	[Writer addInput:mInput.mObject];
}

void Avf::TAssetWriterInput::Finish()
{
	if ( !mInput )
		return;
	
	std::lock_guard<std::mutex> Lock(mLock);
	mFinished = true;
	[mInput.mObject markAsFinished];
}


Avf::TFileMuxer::TFileMuxer(const std::string& Filename,std::shared_ptr<TMediaPacketBuffer>& Input,const std::function<void(bool&)>& OnStreamFinished) :
	TMediaMuxer			( nullptr, Input ),
	mOnStreamFinished	( OnStreamFinished )
{
	//	make asset writer
	@try
	{
		mAssetWriter.reset( new TAssetWriter(Filename) );
	}
	@catch (NSException* e)
	{
		throw Soy::AssertException( Soy::NSErrorToString( e ) );
	}
}

void Avf::TFileMuxer::SetupStreams(const ArrayBridge<TStreamMeta>&& Streams)
{
	//	make writer inputs for each
	for ( int i=0;	i<Streams.GetSize();	i++ )
	{
		auto& Stream = Streams[i];
		@try
		{
			mStreamWriters[Stream.mStreamIndex].reset( new TAssetWriterInput( Stream, *mAssetWriter ) );
		}
		@catch (NSException* e)
		{
			throw Soy::AssertException( Soy::NSErrorToString( e ) );
		}

	}
	
	OnPreWrite( SoyTime() );
}

void Avf::TFileMuxer::OnPreWrite(SoyTime Timecode)
{
	//	already started
	if ( mFirstTimecode.IsValid() )
		return;
	
	mFirstTimecode = Timecode;
	@try
	{
		auto* Writer = mAssetWriter->mAsset.mObject;
		if ( ![Writer startWriting] )
		{
			throw Soy::AssertException( Soy::NSErrorToString( Writer.error ) );
		}
		
		auto FirstTimecodeCm = Soy::Platform::GetTime( mFirstTimecode );
		//[Writer startSessionAtSourceTime:FirstTimecodeCm];
		[Writer startSessionAtSourceTime:kCMTimeZero];
	}
	@catch (NSException* e)
	{
		throw Soy::AssertException( Soy::NSErrorToString( e ) );
	}
}


CMSampleBufferRef CreateSampleBufferFromPixels(TMediaPacket& Packet)
{
	SoyPixelsRemote Pixels( GetArrayBridge(Packet.mData), Packet.mMeta.mPixelMeta );
	CVPixelBufferRef PixelBuffer = Avf::PixelsToPixelBuffer( Pixels );

	CMFormatDescriptionRef NewFormat;
	CFAllocatorRef Allocator = kCFAllocatorDefault;
	auto Result = CMVideoFormatDescriptionCreateForImageBuffer( Allocator, PixelBuffer, &NewFormat );
	Avf::IsOkay( Result, "CMVideoFormatDescriptionCreateForImageBuffer" );
	
	BufferArray<CMSampleTimingInfo,1> SampleTimings;
	auto& FrameTiming = SampleTimings.PushBack();
	FrameTiming.duration = Soy::Platform::GetTime( Packet.mDuration );
	FrameTiming.presentationTimeStamp = Soy::Platform::GetTime( Packet.mTimecode );
	FrameTiming.decodeTimeStamp = Soy::Platform::GetTime( Packet.mDecodeTimecode );

	CMSampleBufferRef NewSampleBuffer = nullptr;
	Result = CMSampleBufferCreateForImageBuffer( Allocator, PixelBuffer, true, nullptr, nullptr, NewFormat, SampleTimings.GetArray(), &NewSampleBuffer );
	Avf::IsOkay( Result, "CMSampleBufferCreateForImageBuffer" );

	return NewSampleBuffer;
}

CMSampleBufferRef CreateSampleBufferFromCompressed(TMediaPacket& Packet,Array<uint8>& Buffer)
{
	auto& AvccData = Buffer;
	
	
	CFAllocatorRef Allocator = nil;
	CMMediaType MediaType;
	FourCharCode MediaCodec;
	Avf::GetMediaType( MediaType, MediaCodec, Packet.mMeta.mCodec );
	CMFormatDescriptionRef Format = nullptr;
	auto Result = CMVideoFormatDescriptionCreate( Allocator, MediaCodec, Packet.mMeta.mPixelMeta.GetWidth(), Packet.mMeta.mPixelMeta.GetHeight(), nullptr, &Format );
	Avf::IsOkay( Result, "CMVideoFormatDescriptionCreate" );
	/*
	 auto& Data = Packet->mData;
	 
	 
	 CMBlockBufferRef videoBlockBuffer = nullptr;
	 CMSampleBufferRef videoSampleBuffer = nullptr;
	 
	 auto Result = CMVideoFormatDescriptionCreate( Allocator, MediaCodec, Packet->mMeta.mPixelMeta.GetWidth(), Packet->mMeta.mPixelMeta.GetHeight(), nullptr, &videoFormat );
	 Avf::IsOkay( Result, "CMVideoFormatDescriptionCreate" );
	 Result = CMBlockBufferCreateWithMemoryBlock(kCFAllocatorDefault, NULL, data.length, kCFAllocatorDefault, NULL, 0, data.length, kCMBlockBufferAssureMemoryNowFlag, &videoBlockBuffer);
	 Avf::IsOkay( Result, "CMBlockBufferCreateWithMemoryBlock" );
	 Result = CMBlockBufferReplaceDataBytes(data.bytes, videoBlockBuffer, 0, data.length);
	 Avf::IsOkay( Result, "CMBlockBufferReplaceDataBytes" );
	 
	 BufferArray<size_t,1> SampleSizes;
	 SampleSizes.PushBack( AvccData.GetDataSize() );
	 BufferArray<CMSampleTimingInfo,1> SampleTimings;
	 auto& FrameTiming = SampleTimings.PushBack();
	 FrameTiming.duration = Soy::Platform::GetTime( Packet.mDuration );
	 FrameTiming.presentationTimeStamp = Soy::Platform::GetTime( Packet.mTimecode );
	 FrameTiming.decodeTimeStamp = Soy::Platform::GetTime( Packet.mDecodeTimecode );
	 
	 Result = CMSampleBufferCreate(kCFAllocatorDefault, videoBlockBuffer, TRUE, NULL, NULL, videoFormat, numberOfSamples, numberOfSampleTimeEntries, &videoSampleTimingInformation, 1, sampleSizeArray, &videoSampleBuffer);
	 Avf::IsOkay( Result, "CMBlockBufferReplaceDataBytes" );
	 
	 Result = CMSampleBufferMakeDataReady(videoSampleBuffer);
	 Avf::IsOkay( Result, "CMBlockBufferReplaceDataBytes" );
	 */
	//	create buffer from packet
	
	CMSampleBufferRef SampleBuffer = nullptr;
	{
		uint32_t SubBlockSize = 0;
		CMBlockBufferFlags Flags = 0;
		CMBlockBufferRef BlockBuffer = nullptr;
		auto Result = CMBlockBufferCreateEmpty( Allocator, SubBlockSize, Flags, &BlockBuffer );
		Avf::IsOkay( Result, "CMBlockBufferCreateEmpty" );
		
		//	for now, just convert. we can speed this up in future by doing 2 explicit buffer appends
		//	gr: using a buffer slows this down??
		//auto& AvccData = Packet.mData;
		AvccData.Copy( Packet.mData );
		
		if ( SoyMediaFormat::IsH264( Packet.mMeta.mCodec ) )
		{
			SoyMediaFormat::Type AvccFormat = Packet.mMeta.mCodec;
			H264::ConvertToFormat( AvccFormat, SoyMediaFormat::H264_32, GetArrayBridge( AvccData ) );
			
			static bool ReconvertTest = false;
			if ( ReconvertTest )
			{
				H264::ConvertToFormat( AvccFormat, SoyMediaFormat::H264_ES, GetArrayBridge( AvccData ) );
				H264::ConvertToFormat( AvccFormat, SoyMediaFormat::H264_32, GetArrayBridge( AvccData ) );
			}
			
			//	gr: due to AUD annexb packets (joins) we can produce empty AVCC packets, safely step over these
			//		we COULD omit them earlier (for cross platform purposes), but cope here anyway
			if ( AvccData.IsEmpty() )
			{
				std::Debug << "Skipping empty AVCC packet" << std::endl;
				return nullptr;
			}
			
			//std::Debug << "AVCC data x" << AvccData.GetDataSize() << "; " << Soy::DataToHexString( GetArrayBridge(AvccData), 20 ) << std::endl;
			
			//	extract first byte
			try
			{
				H264NaluContent::Type Content;
				H264NaluPriority::Type Priority;
				H264::DecodeNaluByte( AvccData[0], Content, Priority );
				std::Debug << "Avcc nalu; " << Content << ", " << Priority << std::endl;
			}
			catch(std::exception& e)
			{
				std::Debug << "Error decoding nalu byte; " << e.what() << std::endl;
			}
		}
		
		//	gr: when you pass memory to a block buffer, it only bloody frees it. make sure kCFAllocatorNull is the "allocator" for the data
		//		also means of course, for async decoding the data could go out of scope. May explain the wierd MACH__O error that came from the decoder?
		void* Data = (void*)AvccData.GetArray();
		auto DataSize = AvccData.GetDataSize();
		size_t Offset = 0;
		
		Result = CMBlockBufferAppendMemoryBlock( BlockBuffer,
												Data,
												DataSize,
												kCFAllocatorNull,
												nullptr,
												Offset,
												DataSize-Offset,
												Flags );
		
		Avf::IsOkay( Result, "CMBlockBufferAppendMemoryBlock" );
		
		
		//CMFormatDescriptionRef Format = GetFormatDescription( Packet.mMeta );
		//CMFormatDescriptionRef Format = Packet.mFormat->mDesc;
		//auto Format = Packet.mFormat ? Packet.mFormat->mDesc : mFormatDesc->mDesc;
		/*
		 if ( !VTDecompressionSessionCanAcceptFormatDescription( mSession->mSession, Format ) )
		 {
			std::Debug << "VTDecompressionSessionCanAcceptFormatDescription failed" << std::endl;
		 }
		 */
		
		int NumSamples = 1;
		BufferArray<size_t,1> SampleSizes;
		SampleSizes.PushBack( AvccData.GetDataSize() );
		BufferArray<CMSampleTimingInfo,1> SampleTimings;
		auto& FrameTiming = SampleTimings.PushBack();
		FrameTiming.duration = Soy::Platform::GetTime( Packet.mDuration );
		FrameTiming.presentationTimeStamp = Soy::Platform::GetTime( Packet.mTimecode );
		FrameTiming.decodeTimeStamp = Soy::Platform::GetTime( Packet.mDecodeTimecode );
		
		static bool ForcedTimecode = false;
		if ( ForcedTimecode )
		{
			static SoyTime Timestamp( 1001ull);
			Timestamp += 30;
			FrameTiming.presentationTimeStamp = Soy::Platform::GetTime( Timestamp );
			FrameTiming.decodeTimeStamp = Soy::Platform::GetTime( Timestamp );
		}
		
		Result = CMSampleBufferCreate(	Allocator,
									  BlockBuffer,
									  true,
									  nullptr,	//	callback
									  nullptr,	//	callback context
									  Format,
									  NumSamples,
									  SampleTimings.GetSize(),
									  SampleTimings.GetArray(),
									  SampleSizes.GetSize(),
									  SampleSizes.GetArray(),
									  &SampleBuffer );
		Avf::IsOkay( Result, "CMSampleBufferCreateReady" );
		
		//	sample buffer now has a reference to the block
		CFRelease( BlockBuffer );
	}
	
	return SampleBuffer;
}


void Avf::TFileMuxer::ProcessPacket(std::shared_ptr<TMediaPacket> pPacket,TStreamWriter& Output)
{
	//	correct some stuff on the packet
	if ( !pPacket->mDuration.IsValid() )
		pPacket->mDuration = SoyTime( 33ull );
	
	//	create buffer from packet
	auto WriterIt = mStreamWriters.find( pPacket->mMeta.mStreamIndex );
	Soy::Assert( WriterIt != mStreamWriters.end(), "Stream writer missing");
	
	auto& WriterWrapper = *WriterIt->second;
	auto* Writer = WriterWrapper.mInput.mObject;
	auto& Packet = *pPacket;
	
	CMSampleBufferRef SampleBuffer = nullptr;
	Array<uint8> AvccData;
	
	if ( SoyMediaFormat::IsPixels( Packet.mMeta.mCodec ) )
	{
		SampleBuffer = CreateSampleBufferFromPixels( Packet );
	}
	else
	{
		SampleBuffer = CreateSampleBufferFromCompressed( Packet, AvccData );
	}

	//	lock here?
	while ( !Writer.isReadyForMoreMediaData )
	{
		if ( WriterWrapper.mFinished )
			throw Soy::AssertException("Avf::TFileMuxer::ProcessPacket writer has been stopped");
		if ( !IsWorking() )
			throw Soy::AssertException("Avf::TFileMuxer::ProcessPacket thread aborted");
		
		std::Debug << "Writer not ready..." << std::endl;
		std::this_thread::sleep_for( std::chrono::milliseconds(20) );
	}
	
	@try
	{
		std::lock_guard<std::mutex> Lock(WriterWrapper.mLock);
		if ( !WriterWrapper.mFinished )
		{
			[Writer appendSampleBuffer:SampleBuffer];
			mLastTimecode = std::max( mLastTimecode, Packet.mTimecode );
		}
	}
	@catch(NSException* e)
	{
		throw Soy::AssertException( Soy::NSErrorToString(e) );
	}
}

void Avf::TFileMuxer::Finish()
{
	Soy::TSemaphore FinishSemaphore;
	__block Soy::TSemaphore& Semaphore = FinishSemaphore;
	auto OnFinishedWriting = ^
	{
		Semaphore.OnCompleted();
	};
	
	@try
	{
		for ( auto it=mStreamWriters.begin();	it!=mStreamWriters.end();	it++ )
		{
			auto& Stream = it->second;
			Stream->Finish();
		}
		auto* Writer = mAssetWriter->mAsset.mObject;

		//	gr: this is optional, so in case there are any bugs... let's skip it
		/*
		auto LastTimecodeCm = Soy::Platform::GetTime( mLastTimecode );
		[Writer endSessionAtSourceTime:LastTimecodeCm];
		 */

		[Writer finishWritingWithCompletionHandler:OnFinishedWriting];
	}
	@catch (NSException* e)
	{
		throw Soy::AssertException( Soy::NSErrorToString( e ) );
	}

	FinishSemaphore.Wait();
	std::Debug << "Writer finished..." << std::endl;
}

