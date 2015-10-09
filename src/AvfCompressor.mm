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



namespace AvfCompressor
{
	void	IsOkay(OSStatus Error,const std::string& Context);
}


class AvfCompressor::TSession
{
public:
	TSession(TInstance& Parent,SoyPixelsMeta OutputMeta);
	~TSession();
	
	void				OnCompressedFrame(CMSampleBufferRef SampleBuffer,VTEncodeInfoFlags Flags);
	void				OnError(const std::string& Error);
	void				Finish();		//	block until encoding has finished
	
public:
	TInstance&				mParent;
	
	//	there is something wierd going on with this type when I try and compile it. so managing raw type rather than using smart pointers
	//ObjcPtr<VTCompressionSessionRef>	mSession;
	VTCompressionSessionRef	mSession;
};


//	lots of errors in macerrors.h with no string conversion :/
#define TESTENUMERROR(e,Enum)	if ( (e) == (Enum)	return ##Enum ;

//	http://stackoverflow.com/questions/2196869/how-do-you-convert-an-iphone-osstatus-code-to-something-useful
std::string GetString(OSStatus Status)
{
//	TESTENUMERROR(Status,x);
	
	//	could be fourcc?
	return Soy::FourCCToString( CFSwapInt32HostToBig(Status) );
	
	NSBundle *bundle = [NSBundle bundleWithIdentifier:@"com.apple.security"];
	NSString *key = [NSString stringWithFormat:@"%d", Status];
	auto* StringNs = [bundle localizedStringForKey:key value:key table:@"SecErrorMessages"];
	return Soy::NSStringToString( StringNs );
}

std::shared_ptr<AvfCompressor::TInstance> AvfCompressor::Allocate(const TCasterParams& Params)
{
	return std::shared_ptr<AvfCompressor::TInstance>( new AvfCompressor::TInstance(Params) );
}


void AvfCompressor::IsOkay(OSStatus Error,const std::string& Context)
{
	if ( Error == noErr )
		return;
	
	std::stringstream ErrorString;
	ErrorString << "OSStatus error in " << Context << ": " << GetString(Error);
	throw Soy::AssertException( ErrorString.str() );
}



//	make compressor VTCompressionSession
void OnCompressionOutput(void *outputCallbackRefCon,
							 void *sourceFrameRefCon,
							  OSStatus status,
								  VTEncodeInfoFlags infoFlags,
								  CMSampleBufferRef sampleBuffer)
{
	auto* This = reinterpret_cast<AvfCompressor::TSession*>(outputCallbackRefCon);
	//	can't assert
	if ( !This )
	{
		std::Debug << "context is null in " << __func__ << std::endl;
		return;
	}

	try
	{
		AvfCompressor::IsOkay( status, "OnCompressionOutput" );
		This->OnCompressedFrame( sampleBuffer, infoFlags );
	}
	catch( std::exception& e )
	{
		This->OnError( e.what() );
	}
};


AvfCompressor::TSession::TSession(TInstance& Parent,SoyPixelsMeta OutputMeta) :
	mParent		( Parent )
{
	NSDictionary *encoderSpec =
	@{
	  (id)kVTVideoEncoderSpecification_EnableHardwareAcceleratedVideoEncoder : @(YES),
	  (id)kVTVideoEncoderSpecification_RequireHardwareAcceleratedVideoEncoder : @(NO),
	  };
	
	auto Codec = kCMVideoCodecType_H264;
	CFAllocatorRef Allocator = nil;
	CFDictionaryRef SourceImageAttribs = nil;
	void* CallbackContext = this;
	
	auto Result = VTCompressionSessionCreate( Allocator, OutputMeta.GetWidth(), OutputMeta.GetHeight(), Codec, (__bridge CFDictionaryRef) encoderSpec, SourceImageAttribs, Allocator, OnCompressionOutput, CallbackContext, &mSession );
	AvfCompressor::IsOkay( Result, "VTCompressionSessionCreate" );

//	VTSessionSetProperty()


	Result = VTCompressionSessionPrepareToEncodeFrames( mSession );
	AvfCompressor::IsOkay( Result, "VTCompressionSessionPrepareToEncodeFrames" );
}


AvfCompressor::TSession::~TSession()
{
	Finish();
	
	if ( mSession )
	{
		VTCompressionSessionInvalidate( mSession );
		CFRelease( mSession );
		mSession = nullptr;
	}
}

void AvfCompressor::TSession::OnCompressedFrame(CMSampleBufferRef SampleBuffer,VTEncodeInfoFlags Flags)
{
	//	turn into frame
	std::shared_ptr<TH264Frame> Frame;
	mParent.PushCompressedFrame( Frame );
}

void AvfCompressor::TSession::OnError(const std::string& Error)
{
	mParent.OnError( Error );
}


void AvfCompressor::TSession::Finish()
{
	if ( !mSession )
		return;
	
	//	block by providing "non-numeric CMTime"
	CMTime EndTime;
	auto Result = VTCompressionSessionCompleteFrames( mSession, EndTime );
	AvfCompressor::IsOkay( Result, "VTCompressionSessionCompleteFrames" );
}



AvfCompressor::TInstance::TInstance(const TCasterParams& Params) :
	mOutputMeta	( 256, 256, SoyPixelsFormat::RGBA )	//	gr: change this from RGBA to H264?
{
	//	allocate session
	mSession.reset( new TSession( *this, mOutputMeta ) );
}


void PixelReleaseCallback(void *releaseRefCon, const void *baseAddress)
{
	
}

void AvfCompressor::TInstance::Write(const Opengl::TTexture& Image,SoyTime Timecode,Opengl::TContext& Context)
{
	throw Soy::AssertException("Not supported yet");
}


void AvfCompressor::TInstance::Write(const std::shared_ptr<SoyPixelsImpl> pImage,SoyTime Timecode)
{
	auto& Image = *pImage;
	Soy::Assert( mSession != nullptr, "Session expected" );

	//	make CVPixelBuffer
	//	send to compression session
	//	callback puts them in frame buffer

	//	grab pool
	auto Pool = VTCompressionSessionGetPixelBufferPool( mSession->mSession );
	
	CVPixelBufferRef PixelBuffer = nullptr;
	{
		CFAllocatorRef PixelBufferAllocator = nullptr;
		OSType PixelFormatType = Soy::Platform::GetFormat( Image.GetFormat() );
		auto& PixelsArray = Image.GetPixelsArray();
		auto* Pixels = const_cast<uint8*>( PixelsArray.GetArray() );
		auto BytesPerRow = Image.GetMeta().GetRowDataSize();
		void* ReleaseContext = nullptr;
		CFDictionaryRef PixelBufferAttributes = nullptr;
		auto Result = CVPixelBufferCreateWithBytes( PixelBufferAllocator, Image.GetWidth(), Image.GetHeight(), PixelFormatType, Pixels, BytesPerRow, PixelReleaseCallback, ReleaseContext, PixelBufferAttributes, &PixelBuffer );
		AvfCompressor::IsOkay( Result, "CVPixelBufferCreateWithBytes" );
	}
	
	{
		CMTime Timestamp = Soy::Platform::GetTime( Timecode );
		CMTime Duration = kCMTimeInvalid;
		CFDictionaryRef FrameProperties = nullptr;
		void* SourceFrameContext = nullptr;
		VTEncodeInfoFlags* FlagsOutput = nullptr;
		auto Result = VTCompressionSessionEncodeFrame( mSession->mSession, PixelBuffer, Timestamp, Duration, FrameProperties, SourceFrameContext, FlagsOutput );
		AvfCompressor::IsOkay( Result, "VTCompressionSessionEncodeFrame" );
	}
}


void AvfCompressor::TInstance::PushCompressedFrame(std::shared_ptr<TH264Frame> Frame)
{

}

void AvfCompressor::TInstance::OnError(const std::string& Error)
{
	
}


