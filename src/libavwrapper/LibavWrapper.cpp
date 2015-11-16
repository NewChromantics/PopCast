extern "C"
{
	#include "avformat.h"
};


void assert()
{
	
}

void av_compare_ts()
{
}

void av_crc()
{
	
}

void av_crc_get_table()
{
	
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

struct AVOutputFormat* av_guess_format(const char* Name,void*,void*)
{
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
	return nullptr;
}


int avformat_write_header(struct AVFormatContext* Context,void*)
{
	//	call the write header func
	//return Context->
	return AVERROR_SUCESS;
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



struct AVOutputFormat* av_guess_format(int Flags,void*,void*)
{
	return nullptr;
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
	return nullptr;
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


