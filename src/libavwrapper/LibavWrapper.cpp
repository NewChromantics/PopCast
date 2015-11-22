extern "C"
{
#include "avformat.h"
#include "libavutil/intreadwrite.h"
	extern AVOutputFormat ff_mpegts_muxer;
	void assert()
	{
		
	}
};
#include "LibavWrapper.h"
#include <SoyStream.h>
#include <SoyString.h>

namespace Libav
{
	template<typename TYPE>
	class TAvWrapperBase;
	template<typename TYPE>
	class TAvWrapper;
	
	template<typename TYPE>
	std::shared_ptr<TAvWrapper<TYPE>>	GetPoolPointer(TYPE* Object,bool Alloc);	//	alloc/find this object's smart pointer
	template<typename TYPE>
	void								FreePoolPointer(TYPE* Object);

	//	alloc and free a libav object
	template<typename TYPE>
	std::shared_ptr<TAvWrapper<TYPE>>	GetPoolPointer(TYPE* Object,bool Alloc);	//	alloc/find this object's smart pointer
	template<typename TYPE>
	void								FreePoolPointer(TYPE* Object);

	std::shared_ptr<Array<uint8>>		AllocPoolArray(size_t Size);	//	alloc some data as an array
	std::shared_ptr<Array<uint8>>		GetPoolArray(void* Data);	//	get shared ptr to existing data
	void								FreePoolArray(void* Data);
	
	namespace Private
	{
		Array<std::shared_ptr<Array<uint8>>>	ArrayPool;
	}
	
	AVCodecID				GetCodecId(SoyMediaFormat::Type Format);
	AVMediaType				GetCodecType(SoyMediaFormat::Type Format);
}

std::ostream& operator<<(std::ostream& out,const AVError& in)
{
	out << (int)(in);
	return out;
}



template<typename TYPE>
class Libav::TAvWrapper : public TAvWrapperBase<TYPE>
{
public:
};


template<>
class Libav::TAvWrapper<struct AVFormatContext> : public TAvWrapperBase<struct AVFormatContext>
{
public:
	~TAvWrapper()
	{
		//	free sub objects
		Libav::FreePoolPointer( mObject.oformat );
		Libav::FreePoolPointer( mObject.pb );
	}

	void	Init(const AVOutputFormat& Format);
	void	SetDefaults(const TStreamMeta& StreamMeta);
	void	AddProgram(AVProgram Program);
	
};


template<>
class Libav::TAvWrapper<struct AVStream> : public TAvWrapperBase<struct AVStream>
{
public:
	~TAvWrapper()
	{
		//	free sub objects
		Libav::FreePoolPointer( mObject.codec );
	}
};


template<>
class Libav::TAvWrapper<struct AVCodec> : public TAvWrapperBase<struct AVCodec>
{
public:
	TAvWrapper();
	~TAvWrapper()
	{
	}
};



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
		//	AV h264 expects annexb
		//case SoyMediaFormat::H264_16:
		//case SoyMediaFormat::H264_8:
		//case SoyMediaFormat::H264_32:
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
			
		default:
			break;
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



void av_opt_set(int& Member,int64_t Value)
{
	Member = size_cast<int>(Value);
}

void av_opt_set(int& Member,int64_t Value,double Min,double Max)
{
	Soy::Assert( Value >= Min && Value < Max, "Setting AvOption out of range");
	Member = size_cast<int>(Value);
}

void av_opt_set(int64_t& Member,int64_t Value,double Min,double Max)
{
	Soy::Assert( Value >= Min && Value < Max, "Setting AvOption out of range");
	Member = Value;
}

void av_opt_set(double& Member,int64_t Value,double Min,double Max)
{
	Soy::Assert( Value >= Min && Value < Max, "Setting AvOption out of range");
	Member = Value;
}

void av_opt_set(float& Member,int64_t Value,double Min,double Max)
{
	Soy::Assert( Value >= Min && Value < Max, "Setting AvOption out of range");
	Member = Value;
}

void av_opt_set(char* Member,const char* Value)
{
	Soy_AssertTodo();
}



void av_opt_set(void* Member,const AVOption& Option)
{
	switch ( Option.type )
	{
		//	read only
		case AV_OPT_TYPE_CONST:
			return;

		//	Cannot set defaults for these types
		case AV_OPT_TYPE_BINARY:
		case AV_OPT_TYPE_DICT:
			return;
		
		default:
			break;

		case AV_OPT_TYPE_FLAGS:
			av_opt_set( *reinterpret_cast<int*>( Member ), Option.default_val.i64 );
			return;
			
		case AV_OPT_TYPE_INT:
			av_opt_set( *reinterpret_cast<int*>( Member ), Option.default_val.i64, Option.min, Option.max );
			return;
			
		case AV_OPT_TYPE_INT64:
			av_opt_set( *reinterpret_cast<int64_t*>( Member ), Option.default_val.i64, Option.min, Option.max );
			return;
			
		case AV_OPT_TYPE_DOUBLE:
			av_opt_set( *reinterpret_cast<double*>( Member ), Option.default_val.dbl, Option.min, Option.max );
			return;
			
		case AV_OPT_TYPE_FLOAT:
			av_opt_set( *reinterpret_cast<float*>( Member ), Option.default_val.dbl, Option.min, Option.max );
			return;
			
		case AV_OPT_TYPE_STRING:
			av_opt_set( reinterpret_cast<char*>(Member), Option.default_val.str );
			return;
			/*
		case AV_OPT_TYPE_RATIONAL:
			 AVRational val;
			 val = av_d2q(opt->default_val.dbl, INT_MAX);
			 av_opt_set_q(s, opt->name, val, 0);
			if ((int)num == num) *(AVRational*)dst= (AVRational){num*intnum, den};
			else                 *(AVRational*)dst= av_d2q(num*intnum/den, 1<<24);
			break;
			 */
	}

	std::stringstream Error;
	Error << "AVOption type " << Option.type << " of option " << Option.name << " not implemented yet" << std::endl;
	throw Soy::AssertException( Error.str() );
}

void av_opt_set_defaults(Array<uint8>& ClassObject,const AVClass& Class)
{
	size_t OptionIndex = 0;
	while ( Class.option[OptionIndex].name )
	{
		auto& Option = Class.option[OptionIndex];
		OptionIndex++;
		
		if ( Option.flags & AV_OPT_FLAG_READONLY )
			continue;
		
		void* Member = ClassObject.GetArray() + Option.offset;
		av_opt_set( Member, Option );
	}
}







void Libav::TAvWrapper<struct AVFormatContext>::SetDefaults(const TStreamMeta& StreamMeta)
{
	auto& Format = mObject;
	
	if ( SoyMediaFormat::IsVideo( StreamMeta.mCodec ) && Format.video_codec_id == AV_CODEC_ID_NONE )
		Format.video_codec_id = GetCodecId( StreamMeta.mCodec );
	
	if ( SoyMediaFormat::IsAudio( StreamMeta.mCodec ) && Format.audio_codec_id == AV_CODEC_ID_NONE )
		Format.audio_codec_id = GetCodecId( StreamMeta.mCodec );
}

void Libav::TAvWrapper<struct AVFormatContext>::AddProgram(AVProgram Program)
{
	
}

void Libav::TAvWrapper<struct AVFormatContext>::Init(const AVOutputFormat& Format)
{
	auto& FormatContext = mObject;
	
	//	alloc private data and initialsie it
	auto priv_data_array = Libav::AllocPoolArray( Format.priv_data_size );
	FormatContext.priv_data = priv_data_array->GetArray();
	av_opt_set_defaults( *priv_data_array, *Format.priv_class );
	
	/*
	if (codec && codec->defaults) {
		int ret;
		const AVCodecDefault *d = codec->defaults;
		while (d->key) {
		 ret = av_opt_set(s, d->key, d->value, 0);
		 av_assert0(ret >= 0);
		 d++;
*/
}


Libav::TAvWrapper<struct AVCodec>::TAvWrapper()
{
	/*
	//mObject.extradata = (void*) 123;
	//	init codec defaults
	auto& c = mObject;
	c.codec_type = codec ? codec->type : AVMEDIA_TYPE_UNKNOWN;
	s->codec      = codec;
	av_opt_set_defaults(s);
		 
		 s->time_base           = (AVRational){0,1};
		 s->framerate           = (AVRational){ 0, 1 };
		 s->get_buffer2         = avcodec_default_get_buffer2;
		 s->get_format          = avcodec_default_get_format;
		 s->execute             = avcodec_default_execute;
		 s->execute2            = avcodec_default_execute2;
		 s->sample_aspect_ratio = (AVRational){0,1};
		 s->pix_fmt             = AV_PIX_FMT_NONE;
		 s->sample_fmt          = AV_SAMPLE_FMT_NONE;
		 
		 s->reordered_opaque    = AV_NOPTS_VALUE;
		 if(codec && codec->priv_data_size){
		 if(!s->priv_data){
		 s->priv_data= av_mallocz(codec->priv_data_size);
		 if (!s->priv_data) {
		 return AVERROR(ENOMEM);
		 }
		 }
		 if(codec->priv_class){
		 *(const AVClass**)s->priv_data = codec->priv_class;
		 av_opt_set_defaults(s->priv_data);
		 }
		 }
		 if (codec && codec->defaults) {
		 int ret;
		 const AVCodecDefault *d = codec->defaults;
		 while (d->key) {
		 ret = av_opt_set(s, d->key, d->value, 0);
		 av_assert0(ret >= 0);
		 d++;
		 }
		 }
		 */
	}

template<typename TYPE>
std::shared_ptr<Libav::TAvWrapper<TYPE>> Libav::GetPoolPointer(TYPE* Object,bool Alloc)
{
	//	may want a lock here sometime
	static Array<std::shared_ptr<TAvWrapper<TYPE>>> Pool;
	for ( int i=0;	i<Pool.GetSize();	i++ )
	{
		auto& Element = Pool[i];
		if ( Element->GetObject() == Object )
			return Element;
	}
	
	//	doesnt exist, alloc
	if ( !Alloc )
		throw Soy::AssertException("Object missing from pool");

	std::shared_ptr<Libav::TAvWrapper<TYPE>> NewObject( new TAvWrapper<TYPE>() );
	
	//	save and double check alloc
	Pool.PushBack( NewObject );
	return GetPoolPointer( NewObject->GetObject(), false );
}


template<typename TYPE>
void Libav::FreePoolPointer(TYPE* Object)
{
	//	may want a lock here sometime
	static Array<std::shared_ptr<TAvWrapper<TYPE>>> Pool;

	std::shared_ptr<Libav::TAvWrapper<TYPE>> pObject;
	for ( int i=0;	i<Pool.GetSize();	i++ )
	{
		auto& Element = Pool[i];
		if ( Element->GetObject() != Object )
			continue;
		
		pObject = Element;
		Pool.RemoveBlock( i, 1 );
		break;
	}
	
	//	doesnt exist, warning
	if ( !pObject )
	{
		std::Debug << __func__ << " warning, missing object from pool" << std::endl;
		return;
	}

	if ( pObject.use_count() > 1 )
	{
		std::Debug << __func__ << " warning, freeing object from pool but refcount " << pObject.use_count() << " > 1" << std::endl;
	}

	//	should dealloc
	pObject.reset();
}


std::shared_ptr<Array<uint8>> Libav::AllocPoolArray(size_t Size)
{
	auto& Pool = Private::ArrayPool;

	std::shared_ptr<Array<uint8>> pArray( new Array<uint8> );
	pArray->SetSize( Size );
	pArray->SetAll( 0 );
	Pool.PushBack( pArray );

	return pArray;
}

std::shared_ptr<Array<uint8>> Libav::GetPoolArray(void* Data)
{
	auto& Pool = Private::ArrayPool;
	
	for ( int i=0;	i<Pool.GetSize();	i++ )
	{
		auto& Array = *Pool[i];
		if ( Array.GetArray() != Data )
			continue;
		
		return Pool[i];
	}
	
	std::stringstream Error;
	Error << "Failed to find pool array";
	throw Soy::AssertException( Error.str() );
}


void Libav::FreePoolArray(void* Data)
{
	if ( !Data )
		return;
	
	auto& Pool = Private::ArrayPool;
	
	for ( int i=0;	i<Pool.GetSize();	i++ )
	{
		auto& Array = *Pool[i];
		if ( Array.GetArray() != Data )
			continue;
		
		Pool.RemoveBlock( i, 1 );
		return;
	}

	std::stringstream Error;
	std::Debug << __func__ << " failed to find pool array" << std::endl;
}


void av_free(void* Object)
{
	Libav::FreePoolArray( Object );
}

void av_freep(void* Object)
{
	av_free( Object );
}

struct AVOutputFormat* av_guess_format(const char* FormatName,const char* Filename,const char* Extension)
{
	//	build map
	std::map<std::string,struct AVOutputFormat*> FormatMap;
	
	Array<struct AVOutputFormat*> AllFormats;
	AllFormats.PushBack( &ff_mpegts_muxer );
	for ( int f=0;	f<AllFormats.GetSize();	f++ )
	{
		auto* Format = AllFormats[f];
		auto UpdateMap = [&](const std::string& Part,const char& Delin)
		{
			FormatMap[Part] = Format;
			return true;
		};
		Soy::StringSplitByMatches( UpdateMap, Format->extensions, ",", false );
	}
	
	return FormatMap[FormatName];
}


void av_log(AVFormatContext *avcl, int level, const char *fmt, ...)
{
	std::Debug << "av_log; " << fmt << std::endl;
}

int av_match_ext(const char* Filename,const char* Ext)
{
	return false;
}


int avcodec_copy_context(const AVFormatContext* Source,struct AVFormatContext* Dest)
{
	return AVERROR_SUCESS;
}


template<typename TYPE>	//	TYPE=TYPE*
void ReallocArray(TYPE*& Data,unsigned int& Size,unsigned int NewSize)
{
	TYPE* OldData = Data;

	if ( !OldData )
		Soy::Assert( Size==0, "No old data, size should be zero");
	
	if ( !OldData )
	{
		if ( NewSize == 0 )
			return;
		
		//	alloc new Array
		BufferArray<TYPE,100>* pArray = new BufferArray<TYPE,100>();
		pArray->SetSize( NewSize );
		Soy::Assert( (void*)pArray == (void*)pArray->GetArray(), "Expecting buffer array to point to itself" );
		Data = reinterpret_cast<TYPE*>(pArray);
		Size = NewSize;
	}
	else if ( NewSize == 0 )
	{
		BufferArray<TYPE,100>* pArray = reinterpret_cast<BufferArray<TYPE,100>*>( OldData );
		delete pArray;
		Data = nullptr;
		Size = 0;
	}
	else
	{
		//	resize existing array
		BufferArray<TYPE,100>* pArray = reinterpret_cast<BufferArray<TYPE,100>*>( OldData );
		auto OldSize = pArray->GetSize();
		for ( size_t i=OldSize;	i<NewSize;	i++ )
			pArray->PushBack( nullptr );
		Size = NewSize;
	}
}


struct AVStream* avformat_new_stream(struct AVFormatContext* Context,const struct AVCodec* Codec)
{
	Soy::Assert( Codec==nullptr, "Handle codec!=null");
	
	auto pStream = Libav::GetPoolPointer<struct AVStream>( nullptr, true )->GetObject();
	
	pStream->priv_data = nullptr;	//	alloc how much is needed by format
	pStream->codec = Libav::GetPoolPointer<struct AVCodec>( nullptr, true )->GetObject();

	//	gr: re-alloc
	ReallocArray( Context->streams, Context->nb_streams, Context->nb_streams+1 );
	Context->streams[Context->nb_streams-1] = pStream;
	
	return pStream;
}


int avformat_write_header(struct AVFormatContext* Context,void*)
{
	//	call the write header func
	return Context->oformat->write_header(Context);
}


void avpriv_set_pts_info(struct AVStream* Stream,int Pts,int,int Base)
{
	
}


struct AVDictionaryEntry* av_dict_get(struct AVDictionary*,const char* Key,void*,int)
{
	return nullptr;
}

void* av_mallocz(size_t Size)
{
	auto Array = Libav::AllocPoolArray(Size);
	return Array->GetArray();
}

void* av_malloc(size_t Size)
{
	auto Array = Libav::AllocPoolArray(Size);
	return Array->GetArray();
}

void* av_realloc(void* Data,size_t Size)
{
	//	get existing array and resize
	auto Array = (!Data) ? Libav::AllocPoolArray(Size) : Libav::GetPoolArray( Data );
	Array->SetSize( Size );
	return Array->GetArray();
}

char* av_strdup(const char* s)
{
	//	deallocated by av_free
	auto Length = strlen(s);
	char* Dupe = reinterpret_cast<char*>( av_malloc( Length ) );
	Soy::Assert( Dupe != nullptr, "Failed to allocate strdup");
	memcpy( Dupe, s, Length );
	return Dupe;
}

/* add one element to a dynamic array */
void ff_dynarray_add(intptr_t **tab_ptr, int *nb_ptr, intptr_t elem)
{
	/* see similar avconv.c:grow_array() */
	int nb, nb_alloc;
	intptr_t *tab;
	
	nb = *nb_ptr;
	tab = *tab_ptr;
	if ((nb & (nb - 1)) == 0) {
		if (nb == 0)
			nb_alloc = 1;
		else
			nb_alloc = nb * 2;
		tab = reinterpret_cast<intptr_t*>( av_realloc(tab, nb_alloc * sizeof(intptr_t)));
		*tab_ptr = tab;
	}
	tab[nb++] = elem;
	*nb_ptr = nb;
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
	return Context->GetObject();
}

void avformat_free_context(struct AVFormatContext* Context)
{
	Soy_AssertTodo();
}

void av_init_packet(struct AVPacket* Packet)
{
	Soy_AssertTodo();
}

int avio_open_dyn_buf(struct AVIOContext** Context)
{
	Soy_AssertTodo();
	return 0;
}

int ffio_free_dyn_buf(struct AVIOContext** Context)
{
	Soy_AssertTodo();
	return 0;
}

size_t avio_close_dyn_buf(struct AVIOContext* Context,void* Data)
{
	Soy_AssertTodo();
	return 0;
}

size_t avio_tell(AVIOContext* ContextIo)
{
	Soy::Assert( ContextIo, "avio_tell AVIOContext expected");
	Soy::Assert( ContextIo->pLibav_TContext, "avio_tell AVIOContext->pLibav_TContext expected");
	
	auto& Context = *reinterpret_cast<Libav::TContext*>( ContextIo->pLibav_TContext );
	return Context.IoTell();
}

void avio_flush(AVIOContext* ContextIo)
{
	Soy::Assert( ContextIo, "avio_tell AVIOContext expected");
	Soy::Assert( ContextIo->pLibav_TContext, "avio_tell AVIOContext->pLibav_TContext expected");
	
	auto& Context = *reinterpret_cast<Libav::TContext*>( ContextIo->pLibav_TContext );
	return Context.IoFlush();
}

void avio_write(AVIOContext* ContextIo,const uint8_t* Data,size_t Size)
{
	//	data to array
	if ( Data && !Size )
		return;
	if ( Size && !Data )
		Soy::Assert( Data!=nullptr, "avio_write size, but missing data");
	if ( !Size && !Data )
		return;
	auto Array = GetRemoteArray( Data, Size );
	
	Soy::Assert( ContextIo, "avio_tell AVIOContext expected");
	Soy::Assert( ContextIo->pLibav_TContext, "avio_tell AVIOContext->pLibav_TContext expected");
	
	auto& Context = *reinterpret_cast<Libav::TContext*>( ContextIo->pLibav_TContext );
	return Context.IoWrite( GetArrayBridge(Array) );
}

int av_write_frame(struct AVFormatContext* Context,struct AVPacket* Packet)
{
	Soy_AssertTodo();
	return 0;
}



const uint8_t *avpriv_find_start_code(const uint8_t * p,
									  const uint8_t *end,
									  uint32_t *  state)
{
	int i;
	
	assert(p <= end);
	if (p >= end)
		return end;
	
	for (i = 0; i < 3; i++) {
		uint32_t tmp = *state << 8;
		*state = tmp + *(p++);
		if (tmp == 0x100 || p == end)
			return p;
	}
	
	while (p < end) {
		if      (p[-1] > 1      ) p += 3;
		else if (p[-2]          ) p += 2;
		else if (p[-3]|(p[-1]-1)) p++;
		else {
			p++;
			break;
		}
	}
	
	p = std::min(p, end) - 4;
	*state = AV_RB32(p);
	
	return p + 4;
}


Libav::TContext::TContext(const std::string& FormatName,std::shared_ptr<TStreamBuffer> Output) :
	mOutput	( Output )
{
	Soy::Assert( mOutput!=nullptr, "Output stream expected");
	
	//	find a format
	auto* Format = av_guess_format( FormatName.c_str(),nullptr,nullptr );
	
	//	alloc context
	mFormat = Libav::GetPoolPointer( avformat_alloc_context(), false );
	mFormat->Init( *Format );
	auto& FormatContext = *mFormat->GetObject();
	FormatContext.oformat = Format;
	FormatContext.pb = Libav::GetPoolPointer<AVIOContext>( nullptr, true )->GetObject();
	FormatContext.pb->pLibav_TContext = this;
	
	mFormat->AddProgram( AVProgram() );
	
}

void Libav::TContext::WriteHeader(const ArrayBridge<TStreamMeta>& Streams)
{
	//	setup streams in context
	for ( int s=0;	s<Streams.GetSize();	s++ )
	{
		auto& StreamSoy = Streams[s];
		
		//	setup some defaults for the format
		mFormat->SetDefaults( StreamSoy );

		auto* StreamAv = avformat_new_stream( mFormat->GetObject(), nullptr );
		
		//	setup stream
		StreamAv->id = StreamSoy.mStreamIndex;
		StreamAv->codec = Libav::GetPoolPointer<struct AVCodec>( nullptr, true )->GetObject();
		StreamAv->codec->codec_id = GetCodecId( StreamSoy.mCodec );
		StreamAv->codec->codec_type = GetCodecType( StreamSoy.mCodec );
		
		
		//	base of millisecs
		StreamAv->time_base = (AVRational){1,1000000};

		//	codec settings
		/*
		StreamAv->bit_rate = 400000;
		StreamAv->width = 800;
		StreamAv->height = 640;
		StreamAv->time_base.den = STREAM_FRAME_RATE;
		StreamAv->time_base.num = 1;
		StreamAv->gop_size = 12; // emit one intra frame every twelve frames at most
		StreamAv->pix_fmt = STREAM_PIX_FMT;
		
		//	AV_CODEC_FLAG_GLOBAL_HEADER
		 */
	}
	
	//	write
	auto& Format = *mFormat->GetObject()->oformat;
	auto Result = Format.write_header( mFormat->GetObject() );
	IsOkay( Result, "Write muxing header" );
}

void Libav::TContext::WritePacket(const Libav::TPacket& Packet)
{
	auto& Format = *mFormat->GetObject()->oformat;
	auto Result = Format.write_packet( mFormat->GetObject(), Packet.mAvPacket->GetObject() );
	IsOkay( Result, "Write packet" );
}


size_t Libav::TContext::IoTell()
{
	Soy_AssertTodo();
	return 0;
}


void Libav::TContext::IoFlush()
{
	//	n/a for stream buffers
}

void Libav::TContext::IoWrite(const ArrayBridge<uint8>&& Data)
{
	mOutput->Push( Data );
}


Libav::TPacket::TPacket(std::shared_ptr<TMediaPacket> Packet) :
	mSoyPacket	( Packet )
{
	Soy::Assert( mSoyPacket != nullptr, "TMediaPacket expected");
	
	//	alloc an avpacket
	mAvPacket = Libav::GetPoolPointer<AVPacket>( nullptr, true );
	auto& AvPacket = *mAvPacket->GetObject();
	AvPacket.stream_index = Packet->mMeta.mStreamIndex;
	AvPacket.size = Packet->mData.GetDataSize();
	AvPacket.data = Packet->mData.GetArray();
	
	static bool AllKeyframes = true;
	
	//	todo: convert these properly to the... codec? stream? format? time scalar
	if ( Packet->mTimecode.IsValid() )
		AvPacket.pts = Packet->mTimecode.GetTime();
	else
		AvPacket.pts = AV_NOPTS_VALUE;
	
	if ( Packet->mDecodeTimecode.IsValid() )
		AvPacket.dts = Packet->mDecodeTimecode.GetTime();
	else
		AvPacket.dts = AV_NOPTS_VALUE;

	AvPacket.flags = 0;
	if ( AllKeyframes || Packet->mIsKeyFrame )
		AvPacket.flags |= AV_PKT_FLAG_KEY;
	
}


