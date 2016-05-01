#include "AvfCompressor.h"

#include <CoreMedia/CMBase.h>
#include <VideoToolbox/VTBase.h>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreVideo/CoreVideo.h>
#include <CoreMedia/CMSampleBuffer.h>
#include <CoreMedia/CMFormatDescription.h>
#include <CoreMedia/CMTime.h>
#include <VideoToolbox/VTSession.h>
#include <VideoToolbox/VTCompressionProperties.h>
#include <VideoToolbox/VTCompressionSession.h>
#include <VideoToolbox/VTErrors.h>

#include "AvfPixelBuffer.h"
#include <SoyStream.h>
#include <SoyMedia.h>
#include <SoyH264.h>
#include "SoyAvf.h"


class Avf::TSession
{
public:
	TSession(TEncoder& Parent,SoyPixelsMeta OutputMeta,TParams& Params,size_t StreamIndex,std::function<void(std::shared_ptr<TMediaPacket>&)> PushFrameFunc);
	~TSession();
	
	void				OnCompressedFrame(CMSampleBufferRef SampleBuffer,VTEncodeInfoFlags Flags);
	void				OnError(const std::string& Error);
	void				Finish();		//	block until encoding has finished
	
public:
	TEncoder&				mParent;
	
	std::function<void(std::shared_ptr<TMediaPacket>&)>	mPushFrameFunc;
	
	//	there is something wierd going on with this type when I try and compile it. so managing raw type rather than using smart pointers
	//ObjcPtr<VTCompressionSessionRef>	mSession;
	VTCompressionSessionRef	mSession;
	size_t					mStreamIndex;
};



//	make compressor VTCompressionSession
void OnCompressionOutput(void *outputCallbackRefCon,
							 void *sourceFrameRefCon,
							  OSStatus status,
								  VTEncodeInfoFlags infoFlags,
								  CMSampleBufferRef sampleBuffer)
{
	if ( infoFlags & kVTEncodeInfo_FrameDropped )
	{
		std::Debug << __func__ << " frame dropped" << std::endl;
		return;
	}
	
	auto* This = reinterpret_cast<Avf::TSession*>(outputCallbackRefCon);
	//	can't assert
	if ( !This )
	{
		std::Debug << "context is null in " << __func__ << std::endl;
		return;
	}

	try
	{
		Avf::IsOkay( status, "OnCompressionOutput" );
		This->OnCompressedFrame( sampleBuffer, infoFlags );
	}
	catch( std::exception& e )
	{
		This->OnError( e.what() );
	}
};


Avf::TSession::TSession(TEncoder& Parent,SoyPixelsMeta OutputMeta,TParams& Params,size_t StreamIndex,std::function<void(std::shared_ptr<TMediaPacket>&)> PushFrameFunc) :
	mParent			( Parent ),
	mStreamIndex	( StreamIndex ),
	mPushFrameFunc	( PushFrameFunc )
{
	NSDictionary *encoderSpec =
	@{
	  (id)kVTVideoEncoderSpecification_EnableHardwareAcceleratedVideoEncoder : @(YES),
	  (id)kVTVideoEncoderSpecification_RequireHardwareAcceleratedVideoEncoder : @(Params.mRequireHardwareAcceleration ? YES : NO),
	  };
	
	auto Codec = kCMVideoCodecType_H264;
	CFAllocatorRef Allocator = nil;
	CFDictionaryRef SourceImageAttribs = nil;
	void* CallbackContext = this;
	
	auto Result = VTCompressionSessionCreate( Allocator, size_cast<int32_t>(OutputMeta.GetWidth()), size_cast<int32_t>(OutputMeta.GetHeight()), Codec, (__bridge CFDictionaryRef) encoderSpec, SourceImageAttribs, Allocator, OnCompressionOutput, CallbackContext, &mSession );
	Avf::IsOkay( Result, "VTCompressionSessionCreate" );

	//	set bitrate
	{
		SInt32 Bitrate32 = size_cast<SInt32>(Params.mAverageBitrate);
		auto numref = CFNumberCreate( nullptr, kCFNumberSInt32Type, &Bitrate32 );
		Soy::Assert( numref != nullptr, "Failed to create CFNumber for bitrate");
		auto Result = VTSessionSetProperty( mSession, kVTCompressionPropertyKey_AverageBitRate, numref );
		CFRelease(numref);
		numref = nullptr;
		
		Avf::IsOkay( Result, "VTSessionSetProperty(kVTCompressionPropertyKey_AverageBitRate)" );
	}

	//	set profile
	{
		auto Profile = Avf::GetProfile( Params.mProfile );
		auto Result = VTSessionSetProperty( mSession, kVTCompressionPropertyKey_ProfileLevel, Profile );
		Avf::IsOkay( Result, "VTSessionSetProperty(kVTCompressionPropertyKey_ProfileLevel)" );
	}
	
	//	enable realtime processing
	{
		auto Realtime = Params.mRealtimeEncoding ? kCFBooleanTrue : kCFBooleanFalse;
		auto Result = VTSessionSetProperty( mSession, kVTCompressionPropertyKey_RealTime, Realtime );
		Avf::IsOkay( Result, "VTSessionSetProperty(kVTCompressionPropertyKey_RealTime)" );
	}
	
	//	set keyframe interval
	{
		SInt32 IntervalSecs = size_cast<SInt32>( Params.mMaxKeyframeInterval.GetTime() / 1000 );
		auto numref = CFNumberCreate(NULL, kCFNumberSInt32Type, &IntervalSecs );
		Soy::Assert( numref != nullptr, "Failed to create CFNumber for interval");
		auto Result = VTSessionSetProperty( mSession, kVTCompressionPropertyKey_MaxKeyFrameIntervalDuration, numref );
		CFRelease(numref);
		numref = nullptr;
		Avf::IsOkay( Result, "VTSessionSetProperty(kVTCompressionPropertyKey_MaxKeyFrameIntervalDuration)" );
	}

	Result = VTCompressionSessionPrepareToEncodeFrames( mSession );
	Avf::IsOkay( Result, "VTCompressionSessionPrepareToEncodeFrames" );
}


Avf::TSession::~TSession()
{
	Finish();
	
	if ( mSession )
	{
		VTCompressionSessionInvalidate( mSession );
		CFRelease( mSession );
		mSession = nullptr;
	}
}




class TNalPacket
{
public:
	TNalPacket(size_t HeaderSize=0) :
		mHeaderBlockSize	( HeaderSize )
	{
	}
	
public:
	Array<uint8>	mData;
	size_t			mHeaderBlockSize;		//	0,0,1 or 0,0,0,1
};


void GetH264NalPackets(ArrayBridge<TNalPacket>&& Packets,CMFormatDescriptionRef& Desc)
{
	size_t ParamCount = 0;
	auto Result = CMVideoFormatDescriptionGetH264ParameterSetAtIndex( Desc, 0, nullptr, nullptr, &ParamCount, nullptr );
	Avf::IsOkay( Result, "Get H264 param 0");
	
	/*
	//	known bug on ios?
	if (status ==
		CoreMediaGlue::kCMFormatDescriptionBridgeError_InvalidParameter) {
		DLOG(WARNING) << " assuming 2 parameter sets and 4 bytes NAL length header";
		pset_count = 2;
		nal_size_field_bytes = 4;
	 */

	for ( int i=0;	i<ParamCount;	i++ )
	{
		const uint8_t* ParamsData = nullptr;;
		size_t ParamsSize = 0;
		auto Result = CMVideoFormatDescriptionGetH264ParameterSetAtIndex( Desc, i, &ParamsData, &ParamsSize, nullptr, nullptr);
		
		Avf::IsOkay( Result, "Failed to get H264 param X" );
	
		TNalPacket Packet(4);
		Packet.mData.PushBackArray( GetRemoteArray( ParamsData, ParamsSize ) );
		Packets.PushBack( Packet );
	}
}


class TAnnexbStream
{
public:
	void			Write(const TNalPacket& Packet);
	
public:
	TStreamBuffer	mBuffer;
};

void TAnnexbStream::Write(const TNalPacket& Packet)
{
	uint8 NalHeader4[4] = {0,0,0,1};
	uint8 NalHeader3[3] = {0,0,1};
	if ( Packet.mHeaderBlockSize == 3 )
		mBuffer.Push( GetArrayBridge( GetRemoteArray(NalHeader3) ) );
	else if ( Packet.mHeaderBlockSize == 4 )
		mBuffer.Push( GetArrayBridge( GetRemoteArray(NalHeader4) ) );
	else
		Soy::Assert( false, "Invalid nal packet header block size");

	mBuffer.Push( GetArrayBridge( Packet.mData ) );
}
	


template <typename NalSizeType>
void GetNalPackets(const ArrayBridge<uint8>&& H264Data,ArrayBridge<TNalPacket>&& Packets)
{
	static_assert(sizeof(NalSizeType) == 1 || sizeof(NalSizeType) == 2 ||
				  sizeof(NalSizeType) == 4,
				  "NAL size type has unsupported size");
	
///	static const char startcode_3[3] = {0, 0, 1};
/*

	TArrayReader Reader( H264Data );

	while ( !Reader.Eod() )
	{
		NalSizeType nal_size;
		Reader.Read( nal_size );
		//base::ReadBigEndian(avcc_buffer, &nal_size);

		TNalPacket Packet(3);
		Reader.Read( Packet.mData, nal_size );
	}
 */
}


void Avf::TSession::OnCompressedFrame(CMSampleBufferRef SampleBuffer,VTEncodeInfoFlags Flags)
{
	bool IsKeyframe = Avf::IsKeyframe(SampleBuffer,false);
	
	if ( IsKeyframe )
	{
		auto SpsPacket = GetFormatDescriptionPacket( SampleBuffer, 0, SoyMediaFormat::H264_SPS_ES, mStreamIndex );
		auto PpsPacket = GetFormatDescriptionPacket( SampleBuffer, 1, SoyMediaFormat::H264_PPS_ES, mStreamIndex );
		mPushFrameFunc( SpsPacket );
		mPushFrameFunc( PpsPacket );
	}

	auto H264Packet = GetH264Packet( SampleBuffer, mStreamIndex );
	mPushFrameFunc( H264Packet );
}
	/*
	
	
	
	//	grab all the h264 params for this buffer (though we may only use them for keyframes, I'm curious

	
	//	copy meta
	auto Desc = CMSampleBufferGetFormatDescription( SampleBuffer );
	auto Fourcc = CFSwapInt32HostToBig( CMFormatDescriptionGetMediaSubType(Desc) );
	auto Codec = SoyMediaFormat::FromFourcc( Fourcc );
	Packet.mMeta.mCodec = Codec;
	//	Packet.mMeta = GetStreamMeta( CMSampleBufferGetFormatDescription(SampleBuffer) );
	Packet.mMeta.mStreamIndex = mStreamIndex;
	
	Packet.mFormat.reset( new Platform::TMediaFormat( Desc ) );
	
	CMTime PresentationTimestamp = CMSampleBufferGetPresentationTimeStamp(SampleBuffer);
	CMTime DecodeTimestamp = CMSampleBufferGetDecodeTimeStamp(SampleBuffer);
	CMTime SampleDuration = CMSampleBufferGetDuration(SampleBuffer);
	Packet.mTimecode = Soy::Platform::GetTime(PresentationTimestamp);
	Packet.mDecodeTimecode = Soy::Platform::GetTime(DecodeTimestamp);
	Packet.mDuration = Soy::Platform::GetTime(SampleDuration);
	//GetPixelBufferManager().mOnFrameFound.OnTriggered( Timestamp );
	
	mPushFrameFunc( pPacket );

	//CMSampleBufferInvalidate( SampleBuffer );
	
	
	
	
	
	
	
	
	
	
	
	//	code based on chromium
	//	https://chromium.googlesource.com/chromium/src/media/+/cea1808de66191f7f1eb48b5579e602c0c781146/cast/sender/h264_vt_encoder.cc
	/*
	auto sample_attachments = static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(
																				  CoreMediaGlue::CMSampleBufferGetSampleAttachmentsArray(sbuf, true), 0));
	// If the NotSync key is not present, it implies Sync, which indicates a
	// keyframe (at least I think, VT documentation is, erm, sparse). Could
	// alternatively use kCMSampleAttachmentKey_DependsOnOthers == false.
	bool keyframe =
	!CFDictionaryContainsKey(sample_attachments,
							 CoreMediaGlue::kCMSampleAttachmentKey_NotSync());
	// Increment the encoder-scoped frame id and assign the new value to this
	// frame. VideoToolbox calls the output callback serially, so this is safe.
	uint32 frame_id = ++encoder->frame_id_;
	scoped_ptr<EncodedFrame> encoded_frame(new EncodedFrame());
	encoded_frame->frame_id = frame_id;
	encoded_frame->reference_time = request->reference_time;
	encoded_frame->rtp_timestamp = request->rtp_timestamp;
	if (keyframe) {
		encoded_frame->dependency = EncodedFrame::KEY;
		encoded_frame->referenced_frame_id = frame_id;
	} else {
		encoded_frame->dependency = EncodedFrame::DEPENDENT;
		// H.264 supports complex frame reference schemes (multiple reference
		// frames, slice references, backward and forward references, etc). Cast
		// doesn't support the concept of forward-referencing frame dependencies or
		// multiple frame dependencies; so pretend that all frames are only
		// decodable after their immediately preceding frame is decoded. This will
		// ensure a Cast receiver only attempts to decode the frames sequentially
		// and in order. Furthermore, the encoder is configured to never use forward
		// references (see |kVTCompressionPropertyKey_AllowFrameReordering|). There
		// is no way to prevent multiple reference frames.
		encoded_frame->referenced_frame_id = frame_id - 1;
	}
*/
	
	//	http://aviadr1.blogspot.co.uk/2010/05/h264-extradata-partially-explained-for.html
	//	annexB is split by 0001 and has no size
	//	0001 SPS { 0001 PPS }

	//	AVCC has a file header
	//	HEADER SPS PPSCOUNT { PPSSIZE16 PPS }
	
	
	/*
	//	grab all the h264 params for this buffer (though we may only use them for keyframes, I'm curious
	auto Desc = CMSampleBufferGetFormatDescription( SampleBuffer );
	Soy::Assert( Desc!=nullptr, std::string(__func__) + " failed to get format description");

	auto Fourcc = CFSwapInt32HostToBig( CMFormatDescriptionGetMediaSubType(Desc) );
	auto Codec = SoyMediaFormat::FromFourcc( Fourcc );

	CMSampleBufferInvalidate( SampleBuffer );
	
	/*
	Array<TNalPacket> NalPackets;
	//	if keyframe
	//	gr: assuming this is picture info (SPS etc)
	GetH264NalPackets( GetArrayBridge(NalPackets), Desc );
 

	//	"copy all nal packets, In the process convert them from AVCC format (length header) to AnnexB format (start code)"
	//	bb_data is buffer contiguious data
	int nal_size_field_bytes = 0;
	auto Result = CMVideoFormatDescriptionGetH264ParameterSetAtIndex( Desc, 0, nullptr, nullptr, nullptr, &nal_size_field_bytes );
	AvfCompressor::IsOkay( Result, "Get H264 param NAL size");
	
	Array<uint8> H264Data;

	if (nal_size_field_bytes == 1)
	{
		GetNalPackets<uint8_t>( GetArrayBridge(H264Data), GetArrayBridge(NalPackets) );
	}
	else if (nal_size_field_bytes == 2)
	{
		GetNalPackets<uint16_t>( GetArrayBridge(H264Data), GetArrayBridge(NalPackets) );
	}
	else if (nal_size_field_bytes == 4)
	{
		GetNalPackets<uint32_t>( GetArrayBridge(H264Data), GetArrayBridge(NalPackets) );
	}
	else
	{
		Soy::Assert(false, "invalid nal-size value");
	}
	
	//	if keyframe, write all nalpackets to Annexb (stream)
	TAnnexbStream Annexb;
	for ( int p=0;	p<NalPackets.GetSize();	p++ )
		Annexb.Write( NalPackets[p] );

	
	
	
	
	
	
	//	get the image buffer
	CFPtr<CVImageBufferRef> LockedImageBuffer( CMSampleBufferGetImageBuffer(SampleBuffer), false );
	Soy::Assert( LockedImageBuffer, "Failed to lock image buffer from sample");
	auto& ImageBuffer = LockedImageBuffer.mObject;
	//ImageBuffer.des
	
	
	//	uou want to send SampleBuffer out over the network, which means you may need to switch these over to Elementary Stream packaging.
	//	he parameters sets will in your MPEG-4 package, H.264 will be in the CMVideoFormatDescription.
	//	extract those parameter sets and package them as NAL Units to send out over the network.
	//	CMVideoFormatDescription GetH.264ParameterSetAtIndex.
	//	All right, and the next thing you need to do is the opposite of what we did with AVSampleBufferDisplayLayer.
	//	Our NAL Units are all going to have length headers and you're going to need to convert those length headers into start codes.
	//	So as you extract each NAL Unit from the compressed data inside the CMSampleBuffer, convert those headers on the NAL Units.
	
	SoyTime DecodeTime = Soy::Platform::GetTime( CMSampleBufferGetOutputDecodeTimeStamp( SampleBuffer ) );
	SoyTime PresentationTime = Soy::Platform::GetTime( CMSampleBufferGetOutputPresentationTimeStamp( SampleBuffer ) );
	
	
	auto DataIsReady = CMSampleBufferDataIsReady( SampleBuffer );
	
	CMFormatDescriptionRef FormatDescription = CMSampleBufferGetFormatDescription( SampleBuffer );
	auto Codec = Soy::Platform::GetCodec( FormatDescription );
	auto Extensions = Soy::Platform::GetExtensions( FormatDescription );
	
	
	//	grab pixels
	bool ReadOnlyLock = true;
	auto Error = CVPixelBufferLockBaseAddress( ImageBuffer, ReadOnlyLock ? kCVPixelBufferLock_ReadOnly : 0 );
	AvfCompressor::IsOkay( Error, "CVPixelBufferLockBaseAddress" );

	try
	{
		auto Height = CVPixelBufferGetHeight( ImageBuffer );
		auto Width = CVPixelBufferGetWidth( ImageBuffer );
		auto* Pixels = CVPixelBufferGetBaseAddress(ImageBuffer);
		auto DataSize = CVPixelBufferGetDataSize(ImageBuffer);
		Soy::Assert( Pixels, "Failed to get base address of pixels" );

		//SoyPixelsRemote Temp( reinterpret_cast<uint8*>(Pixels), Width, Height, DataSize, mFormat );
		//mLockedPixels[0] = Temp;
		//Planes.PushBack( &mLockedPixels[0] );
	}
	catch(std::exception& e)
	{
		std::Debug << "Pixel buffer error: " << e.what() << std::endl;
	}
	
	
	std::Debug << "OnCompressedFrame @" << DecodeTime << "/" << PresentationTime << ", ";
	std::Debug << "Data is ready: " << DataIsReady << ", ";
	std::Debug << "codec=" << Codec << ", ";
	std::Debug << "Extensions=" << Codec << ", ";
	std::Debug << std::endl;
	
	
	
	CMSampleBufferInvalidate( SampleBuffer );
/*
	
	
	CreateVideo
	CMVideoFormatDescription(
	
	SampleBuffer->formatDe
	
	SampleBuffer
	CMVideoFormatDescription
	
	
	
	
							 //	turn into frame
							 std::shared_ptr<TH264Frame> Frame;
							 

	mParent.PushCompressedFrame( Frame );
	

		CMSampleBufferInvalidate(mSample.mObject);
*/
	


void Avf::TSession::OnError(const std::string& Error)
{
	mParent.OnError( Error );
}


void Avf::TSession::Finish()
{
	if ( !mSession )
		return;
	
	//	block by providing "non-numeric CMTime"
	CMTime EndTime;
	auto Result = VTCompressionSessionCompleteFrames( mSession, EndTime );
	Avf::IsOkay( Result, "VTCompressionSessionCompleteFrames" );
}



Avf::TEncoder::TEncoder(const TCasterParams& Params,std::shared_ptr<TMediaPacketBuffer>& OutputBuffer,size_t StreamIndex) :
	TMediaEncoder	( OutputBuffer ),
	mOutputMeta		( 256, 256, SoyPixelsFormat::RGBA )	//	gr: change this from RGBA to H264?
{
	//	allocate session
	Avf::TParams CompressorParams;
	
	//	todo: add destruction lock here
	auto Block = []
	{
		return true;
	};
	
	auto PushFrameWrapper = [this,&Block](std::shared_ptr<TMediaPacket>& Frame)
	{
		this->PushFrame( Frame, Block );
	};
	
	mSession.reset( new TSession( *this, mOutputMeta, CompressorParams, StreamIndex, PushFrameWrapper ) );
}


void Avf::TEncoder::Write(const Opengl::TTexture& Image,SoyTime Timecode,Opengl::TContext& Context)
{
	throw Soy::AssertException("Not supported yet");
}


void Avf::TEncoder::Write(std::shared_ptr<SoyPixelsImpl> pImage,SoyTime Timecode)
{
	auto& Image = *pImage;
	Soy::Assert( mSession != nullptr, "Session expected" );

	//	make CVPixelBuffer
	//	send to compression session
	//	callback puts them in frame buffer

	//	grab pool
	auto Pool = VTCompressionSessionGetPixelBufferPool( mSession->mSession );
	
	CVPixelBufferRef PixelBuffer = Avf::PixelsToPixelBuffer( Image );
	
	{
		/*
		 base::ScopedCFTypeRef<CFDictionaryRef> frame_props;
		 if (encode_next_frame_as_keyframe_) {
		 frame_props = DictionaryWithKeyValue(
		 videotoolbox_glue_->kVTEncodeFrameOptionKey_ForceKeyFrame(),
		 kCFBooleanTrue);
		 encode_next_frame_as_keyframe_ = false;
		 }
		 */
		CMTime Timestamp = Soy::Platform::GetTime( Timecode );
		CMTime Duration = kCMTimeInvalid;
		CFDictionaryRef FrameProperties = nullptr;
		void* SourceFrameContext = nullptr;
		VTEncodeInfoFlags FlagsOutput;
		auto Result = VTCompressionSessionEncodeFrame( mSession->mSession, PixelBuffer, Timestamp, Duration, FrameProperties, SourceFrameContext, &FlagsOutput );
		Avf::IsOkay( Result, "VTCompressionSessionEncodeFrame" );
	
		//	detect skipped frame
		if ( FlagsOutput & kVTEncodeInfo_FrameDropped )
		{
			std::Debug << "Videotoolbox dropped frame " << Timecode << std::endl;
			return;
		}
	}
}


void Avf::TEncoder::OnError(const std::string& Error)
{
	mFatalError << Error;
}


