#pragma once


#include <SoyMedia.h>
#include <SoyH264.h>

namespace Avf
{
#if defined(__OBJC__)
	std::shared_ptr<TMediaPacket>	GetH264Packet(CMSampleBufferRef SampleBuffer,size_t StreamIndex);
	std::shared_ptr<TMediaPacket>	GetFormatDescriptionPacket(CMSampleBufferRef SampleBuffer,size_t ParamIndex,SoyMediaFormat::Type Format);
	void							GetFormatDescriptionData(ArrayBridge<uint8>&& Data,CMFormatDescriptionRef FormatDesc,size_t ParamIndex,SoyMediaFormat::Type Format);
	TStreamMeta						GetStreamMeta(CMFormatDescriptionRef FormatDesc);
	CMFormatDescriptionRef			GetFormatDescription(const TStreamMeta& Stream);
	void							GetMediaType(CMMediaType& MediaType,FourCharCode& MediaCodec,SoyMediaFormat::Type Format);
	CFStringRef						GetProfile(H264Profile::Type Profile,Soy::TVersion Level);

	//	OSStatus == CVReturn
	bool							IsOkay(OSStatus Error,const std::string& Context,bool Throw=true);
	std::string						GetString(OSStatus Status);
	CFStringRef						GetProfile(H264Profile::Type Profile);
#endif
}


#if defined(__OBJC__)
class Platform::TMediaFormat
{
public:
	TMediaFormat(CMFormatDescriptionRef Desc) :
	mDesc	( Desc )
	{
		CFRetain( mDesc );
	}
	~TMediaFormat()
	{
		CFRelease( mDesc );
	}
	
	
	CMFormatDescriptionRef	mDesc;
};
#endif

