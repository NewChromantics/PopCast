extern "C"
{
#include "avformat.h"
	extern AVOutputFormat ff_mpegts_muxer;
	void assert()
	{
		
	}
};
#include "LibavWrapper.h"


namespace Libav
{
	template<typename TYPE>
	std::shared_ptr<TYPE>	GetPoolPointer(TYPE* Object,bool Alloc);	//	alloc/find this object's smart pointer

	AVCodecID				GetCodecId(SoyMediaFormat::Type Format);
	AVMediaType				GetCodecType(SoyMediaFormat::Type Format);
}

std::ostream& operator<<(std::ostream& out,const AVError& in)
{
	out << (int)(in);
	return out;
}

void Libav::IsOkay(int LibavError,const std::string& Context,bool Throw)
{
	AVError Error = static_cast<AVError>(LibavError);
	
	if ( Error == AVERROR_SUCESS )
		return;
	
	std::stringstream ErrorStr;
	ErrorStr << "Lib av error in " << Context << "; " << Error << std::endl;
	
}

AVCodecID Libav::GetCodecId(SoyMediaFormat::Type Format)
{
	switch ( Format )
	{
		case SoyMediaFormat::H264_16:
		case SoyMediaFormat::H264_8:
		case SoyMediaFormat::H264_32:
		case SoyMediaFormat::H264_ES:
		case SoyMediaFormat::H264_PPS_ES:
		case SoyMediaFormat::H264_SPS_ES:
			return AV_CODEC_ID_H264;
			
		case SoyMediaFormat::Aac:
			return AV_CODEC_ID_AAC;
			/*
			AV_CODEC_ID_MPEG1VIDEO,
			AV_CODEC_ID_MPEG2VIDEO,
			AV_CODEC_ID_MPEG4,
			AV_CODEC_ID_H264,
			AV_CODEC_ID_HEVC,
			AV_CODEC_ID_CAVS,
			AV_CODEC_ID_DIRAC,
			AV_CODEC_ID_MP2,
			AV_CODEC_ID_MP3,
			AV_CODEC_ID_AAC,
			AV_CODEC_ID_AAC_LATM,
			AV_CODEC_ID_AC3,
			 */
	};
	
	std::stringstream Error;
	Error << __func__ << " unhandled " << Format;
	throw Soy::AssertException( Error.str() );
}

AVMediaType Libav::GetCodecType(SoyMediaFormat::Type Format)
{
	if ( SoyMediaFormat::IsAudio(Format) )
		return AVMEDIA_TYPE_AUDIO;

	if ( SoyMediaFormat::IsVideo(Format) )
		return AVMEDIA_TYPE_VIDEO;
	
	if ( Format == SoyMediaFormat::Subtitle )
		return AVMEDIA_TYPE_SUBTITLE;
	
	std::stringstream Error;
	Error << __func__ << " unhandled " << Format;
	throw Soy::AssertException( Error.str() );
}


template<typename TYPE>
std::shared_ptr<TYPE> Libav::GetPoolPointer(TYPE* Object,bool Alloc)
{
	//	may want a lock here sometime
	static Array<std::shared_ptr<TYPE>> Pool;
	for ( int i=0;	i<Pool.GetSize();	i++ )
	{
		auto& Element = Pool[i];
		if ( Element.get() == Object )
			return Element;
	}
	
	//	doesnt exist, alloc
	if ( !Alloc )
		throw Soy::AssertException("Object missing from pool");

	std::shared_ptr<TYPE> NewObject( new TYPE );
	//	clear C object...
	memset( NewObject.get(), 0, sizeof(TYPE) );
	
	//	save and double check alloc
	Pool.PushBack( NewObject );
	return GetPoolPointer( NewObject.get(), false );
}

void av_dict_get()
{
	
}

void av_free(void* Object)
{
	
}

void av_freep(void* Object)
{
	
}

struct AVOutputFormat* av_guess_format(const char* FormatName,const char* Filename,const char* Extension)
{
	if ( std::string(FormatName) == "ts" )
	{
		return &ff_mpegts_muxer;
	}
	return nullptr;
}

void av_init_packet()
{
	
}

void av_log(void *avcl, int level, const char *fmt, ...)
{
	
}


void av_malloc()
{
}


void av_mallocz()
{
}

int av_match_ext(const char* Filename,const char* Ext)
{
	return false;
}

void av_rescale()
{
	
}


void _av_strdup()
{
	
}


void av_write_frame()
{
	
}


int avcodec_copy_context(const AVFormatContext* Source,struct AVFormatContext* Dest)
{
	return AVERROR_SUCESS;
}


void avformat_free_context()
{
	
}


struct AVStream* avformat_new_stream(struct AVFormatContext* Context,void*)
{
	auto pStream = Libav::GetPoolPointer<struct AVStream>( nullptr, true );
	
	pStream->priv_data = nullptr;	//	alloc how much is needed by format

	//	gr: re-alloc
	Context->streams[Context->nb_streams] = pStream.get();
	Context->nb_streams++;
	
	return pStream.get();
}


int avformat_write_header(struct AVFormatContext* Context,void*)
{
	//	call the write header func
	return Context->oformat->write_header(Context);
}


void avio_close_dyn_buf()
{
	
}


void avio_flush()
{
}


void avio_open_dyn_buf()
{
	
}


void avio_tell()
{
	
}


void avio_write()
{
	
}


void avpriv_find_start_code()
{
	
}


void avpriv_set_pts_info(struct AVStream* Stream,int Pts,int,int Base)
{
	
}


void dynarray_add()
{
	
}


void ffio_free_dyn_buf()
{
	
}



struct AVDictionaryEntry* av_dict_get(struct AVDictionary*,const char* Key,void*,int)
{
	return nullptr;
}

void* av_mallocz(size_t Size)
{
	return nullptr;
}

void* av_malloc(size_t Size)
{
	return nullptr;
}

char* av_strdup(const char* s)
{
	return nullptr;
}

void dynarray_add(intptr_t** Items, int* Counter,intptr_t NewItem)
{
	
}

int64_t av_rescale(size_t Pos,uint64_t Time, uint64_t TimeBase)
{
	return 0;
}

void avpriv_set_pts_info(struct AVSTream* Stream,int Pts,int,int Base)
{
	
}

int av_compare_ts(int64_t Timestamp,struct AVRational Base,int64_t Delay,int Flag)
{
	return 0;
}

struct AVFormatContext* avformat_alloc_context()
{
	auto Context = Libav::GetPoolPointer<struct AVFormatContext>( nullptr, true );
	return Context.get();
}

void avformat_free_context(struct AVFormatContext* Context)
{
}

void av_init_packet(struct AVPacket* Packet)
{
	
}

int avio_open_dyn_buf(struct AVIOContext** Context)
{
	return 0;
}

int ffio_free_dyn_buf(struct AVIOContext** Context)
{
	return 0;
}

size_t avio_close_dyn_buf(struct AVIOContext* Context,void* Data)
{
	return 0;
}

size_t avio_tell(AVIOContext* Context)
{
	return 0;
}

void avio_flush(AVIOContext* Context)
{
}

void avio_write(AVIOContext* Context,const uint8_t* Data,size_t Size)
{
	
}

int av_write_frame(struct AVFormatContext* Context,struct AVPacket* Packet)
{
	return 0;
}


const uint8_t* avpriv_find_start_code(const uint8_t* Start,const uint8_t* End,uint32_t* State)
{
	return nullptr;
}



Libav::TContext::TContext(const std::string& FormatName,std::shared_ptr<TStreamBuffer> Output) :
	mOutput	( Output )
{
	Soy::Assert( mOutput!=nullptr, "Output stream expected");
	
	//	find a format
	auto* Format = av_guess_format( FormatName.c_str(),nullptr,nullptr );
	
	//	alloc context
	mFormat = Libav::GetPoolPointer( avformat_alloc_context(), false );
	mFormat->oformat = Format;
}

void Libav::TContext::WriteHeader(const ArrayBridge<TStreamMeta>& Streams)
{
	//	setup streams in context
	for ( int s=0;	s<Streams.GetSize();	s++ )
	{
		auto& StreamSoy = Streams[s];
		auto* StreamAv = avformat_new_stream( mFormat.get(), nullptr );
		
		//	setup stream
		StreamAv->id = StreamSoy.mStreamIndex;
		StreamAv->codec = Libav::GetPoolPointer<struct AVCodec>( nullptr, true ).get();
		StreamAv->codec->codec_id = GetCodecId( StreamSoy.mCodec );
		StreamAv->codec->codec_type = GetCodecType( StreamSoy.mCodec );
		
	}
	
	//	write
	auto& Format = *mFormat->oformat;
	auto Result = Format.write_header( mFormat.get() );
	IsOkay( Result, "Write muxing header" );
}

void Libav::TContext::WritePacket(const Libav::TPacket& Packet)
{
	auto& Format = *mFormat->oformat;
	auto Result = Format.write_packet( mFormat.get(), Packet.mAvPacket.get() );
	IsOkay( Result, "Write packet" );
}


Libav::TPacket::TPacket(std::shared_ptr<TMediaPacket> Packet) :
	mSoyPacket	( Packet )
{
	//	alloc an avpacket
}
