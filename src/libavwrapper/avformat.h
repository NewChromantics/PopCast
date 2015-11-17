#pragma once

#include <stdint.h>
#include <string.h>	//	memcpy
#define INT_MAX		100

#define AV_DISPOSITION_DEFAULT   0x0001
#define AV_DISPOSITION_DUB       0x0002
#define AV_DISPOSITION_ORIGINAL  0x0004
#define AV_DISPOSITION_COMMENT   0x0008
#define AV_DISPOSITION_LYRICS    0x0010
#define AV_DISPOSITION_KARAOKE   0x0020
#define AV_DISPOSITION_FORCED    0x0040
#define AV_DISPOSITION_HEARING_IMPAIRED  0x0080  /**< stream for hearing impaired audiences */
#define AV_DISPOSITION_VISUAL_IMPAIRED   0x0100  /**< stream for visual impaired audiences */
#define AV_DISPOSITION_CLEAN_EFFECTS     0x0200  /**< stream without voice */

#define AV_PKT_FLAG_KEY		(1<<0)

#define AVFMT_ALLOW_FLUSH	(1<<0)
#define AVFMT_VARIABLE_FPS	(1<<1)

//	time
#define AV_NOPTS_VALUE	-1		//	missing timecode
#define AV_TIME_BASE	0
#define AV_TIME_BASE_Q	0


#define AV_LOG_ERROR	0
#define AV_LOG_WARNING	1
#define AV_LOG_VERBOSE	2
#define AV_LOG_TRACE	3

enum AVError
{
	AVERROR_SUCESS=0,
	AVERROR_INVALIDDATA,
	AVERROR_ENOMEM,
	AVERROR_EINVAL,
};
#define AVERROR(e)	AVERROR_##e

enum AVStreamType
{
	STREAM_TYPE_VIDEO_MPEG2,
	STREAM_TYPE_VIDEO_CAVS,
	STREAM_TYPE_VIDEO_MPEG4,
	STREAM_TYPE_VIDEO_H264,
	STREAM_TYPE_VIDEO_HEVC,
	STREAM_TYPE_VIDEO_DIRAC,
	STREAM_TYPE_AUDIO_MPEG1,
	STREAM_TYPE_AUDIO_AAC_LATM,
	STREAM_TYPE_AUDIO_AAC,
};

enum AVCodecID
{
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
};

enum AVMediaType
{
	AVMEDIA_TYPE_AUDIO,
	AVMEDIA_TYPE_SUBTITLE,
	AVMEDIA_TYPE_VIDEO,
};

enum AVOptionType
{
	AV_OPT_TYPE_INT,
	AV_OPT_TYPE_FLAGS,
	AV_OPT_TYPE_CONST,
};
#define NULL_IF_CONFIG_SMALL(x)	x

#define av_default_item_name	"av_default_item_name"
#define LIBAVUTIL_VERSION_INT	0


typedef struct AVPacket {
	int		stream_index;
	int		size;
	void*	data;
	int		pts;
	int		dts;
	int		flags;	//	AV_PKT_FLAG
} AVPacket;


typedef struct AVDictionary {
	
} AVDictionary;

typedef struct AVDictionaryEntry {
	char*	value;
} AVDictionaryEntry;


typedef struct AVRational{
	int num; ///< numerator
	int den; ///< denominator
} AVRational;



typedef struct AVCodec {
	enum AVCodecID			codec_id;
	enum AVMediaType		codec_type;
	int						extradata_size;
	void*					extradata;
	int						frame_size;
	int						sample_rate;
} AVCodec;

typedef struct AVStream {
	void*					priv_data;
	struct AVDictionary*	metadata;
	struct AVCodec*			codec;		//	actually avcodeccontext
	int						disposition;// AV_DISPOSITION_
	AVRational				time_base;
	int						id;			//	pid
} AVStream;


typedef struct AVOption {
	const char *name;
	
	/**
	 * short English help text
	 * @todo What about other languages?
	 */
	const char *help;
	
	/**
	 * The offset relative to the context structure where the option
	 * value is stored. It should be 0 for named constants.
	 */
	int offset;
	enum AVOptionType type;
	
	/**
	 * the default value for scalar options
	 */
	union {
		int64_t i64;
		double dbl;
		const char *str;
		/* TODO those are unused now */
		AVRational q;
	} default_val;
	double min;                 ///< minimum valid value for the option
	double max;                 ///< maximum valid value for the option
	
	int flags;
#define AV_OPT_FLAG_ENCODING_PARAM  1   ///< a generic parameter which can be set by the user for muxing or encoding
#define AV_OPT_FLAG_DECODING_PARAM  2   ///< a generic parameter which can be set by the user for demuxing or decoding
#if FF_API_OPT_TYPE_METADATA
#define AV_OPT_FLAG_METADATA        4   ///< some data extracted or inserted into the file like title, comment, ...
#endif
#define AV_OPT_FLAG_AUDIO_PARAM     8
#define AV_OPT_FLAG_VIDEO_PARAM     16
#define AV_OPT_FLAG_SUBTITLE_PARAM  32
	/**
	 * The option is inteded for exporting values to the caller.
	 */
#define AV_OPT_FLAG_EXPORT          64
	/**
	 * The option may not be set through the AVOptions API, only read.
	 * This flag only makes sense when AV_OPT_FLAG_EXPORT is also set.
	 */
#define AV_OPT_FLAG_READONLY        128
	//FIXME think about enc-audio, ... style flags
	
	/**
	 * The logical unit to which the option belongs. Non-constant
	 * options and corresponding named constants share the same
	 * unit. May be NULL.
	 */
	const char *unit;
} AVOption;



typedef struct AVClass {
	const char*		class_name;
	const char*		item_name;
	const AVOption*	option;
	int				version;
} AVClass;

typedef struct AVIOContext {
	void*			pLibav_TContext;	//	unsafe Libav::TContext
} AVIOContext;

typedef struct AVProgram {
	
} AVProgram;

typedef struct AVChapter {
	
} AVChapter;


typedef struct AVIOInterruptCB {
	
} AVIOInterruptCB;

typedef struct AVFormatInternal {
	
} AVFormatInternal;

typedef struct AVOutputFormat {
	const char *name;
	/**
	 * Descriptive name for the format, meant to be more human-readable
	 * than name. You should use the NULL_IF_CONFIG_SMALL() macro
	 * to define it.
	 */
	const char *long_name;
	const char *mime_type;
	const char *extensions; /**< comma-separated filename extensions */
	/* output support */
	enum AVCodecID audio_codec;    /**< default audio codec */
	enum AVCodecID video_codec;    /**< default video codec */
	enum AVCodecID subtitle_codec; /**< default subtitle codec */
	/**
	 * can use flags: AVFMT_NOFILE, AVFMT_NEEDNUMBER,
	 * AVFMT_GLOBALHEADER, AVFMT_NOTIMESTAMPS, AVFMT_VARIABLE_FPS,
	 * AVFMT_NODIMENSIONS, AVFMT_NOSTREAMS, AVFMT_ALLOW_FLUSH,
	 * AVFMT_TS_NONSTRICT
	 */
	int flags;
	
	/**
	 * List of supported codec_id-codec_tag pairs, ordered by "better
	 * choice first". The arrays are all terminated by AV_CODEC_ID_NONE.
	 */
	const struct AVCodecTag * const *codec_tag;
	
	
	const AVClass *priv_class; ///< AVClass for the private context
	
	/*****************************************************************
	 * No fields below this line are part of the public API. They
	 * may not be used outside of libavformat and can be changed and
	 * removed at will.
	 * New public fields should be added right above.
	 *****************************************************************
	 */
	struct AVOutputFormat *next;
	/**
	 * size of private data so that it can be allocated in the wrapper
	 */
	int priv_data_size;
	
	int (*write_header)(struct AVFormatContext *);
	/**
	 * Write a packet. If AVFMT_ALLOW_FLUSH is set in flags,
	 * pkt can be NULL in order to flush data buffered in the muxer.
	 * When flushing, return 0 if there still is more data to flush,
	 * or 1 if everything was flushed and there is no more buffered
	 * data.
	 */
	int (*write_packet)(struct AVFormatContext *, AVPacket *pkt);
	int (*write_trailer)(struct AVFormatContext *);
	/**
	 * Currently only used to set pixel format if not YUV420P.
	 */
	int (*interleave_packet)(struct AVFormatContext *, AVPacket *out,
							 AVPacket *in, int flush);
	/**
	 * Test if the given codec can be stored in this container.
	 *
	 * @return 1 if the codec is supported, 0 if it is not.
	 *         A negative number if unknown.
	 */
	int (*query_codec)(enum AVCodecID id, int std_compliance);
} AVOutputFormat;


typedef struct AVFormatContext {
	
	/**
	 * A class for logging and @ref avoptions. Set by avformat_alloc_context().
	 * Exports (de)muxer private options if they exist.
	 */
	const AVClass *av_class;
	
	/**
	 * The input container format.
	 *
	 * Demuxing only, set by avformat_open_input().
	 */
	struct AVInputFormat *iformat;
	
	/**
	 * The output container format.
	 *
	 * Muxing only, must be set by the caller before avformat_write_header().
	 */
	struct AVOutputFormat *oformat;
	
	/**
	 * Format private data. This is an AVOptions-enabled struct
	 * if and only if iformat/oformat.priv_class is not NULL.
	 *
	 * - muxing: set by avformat_write_header()
	 * - demuxing: set by avformat_open_input()
	 */
	void *priv_data;
	
	/**
	 * I/O context.
	 *
	 * - demuxing: either set by the user before avformat_open_input() (then
	 *             the user must close it manually) or set by avformat_open_input().
	 * - muxing: set by the user before avformat_write_header(). The caller must
	 *           take care of closing / freeing the IO context.
	 *
	 * Do NOT set this field if AVFMT_NOFILE flag is set in
	 * iformat/oformat.flags. In such a case, the (de)muxer will handle
	 * I/O in some other way and this field will be NULL.
	 */
	AVIOContext *pb;
	
	/* stream info */
	/**
	 * Flags signalling stream properties. A combination of AVFMTCTX_*.
	 * Set by libavformat.
	 */
	int ctx_flags;
	
	/**
	 * Number of elements in AVFormatContext.streams.
	 *
	 * Set by avformat_new_stream(), must not be modified by any other code.
	 */
	unsigned int nb_streams;
	/**
	 * A list of all streams in the file. New streams are created with
	 * avformat_new_stream().
	 *
	 * - demuxing: streams are created by libavformat in avformat_open_input().
	 *             If AVFMTCTX_NOHEADER is set in ctx_flags, then new streams may also
	 *             appear in av_read_frame().
	 * - muxing: streams are created by the user before avformat_write_header().
	 *
	 * Freed by libavformat in avformat_free_context().
	 */
	AVStream **streams;
	
	/**
	 * input or output filename
	 *
	 * - demuxing: set by avformat_open_input()
	 * - muxing: may be set by the caller before avformat_write_header()
	 */
	char filename[1024];
	
	/**
	 * Position of the first frame of the component, in
	 * AV_TIME_BASE fractional seconds. NEVER set this value directly:
	 * It is deduced from the AVStream values.
	 *
	 * Demuxing only, set by libavformat.
	 */
	int64_t start_time;
	
	/**
	 * Duration of the stream, in AV_TIME_BASE fractional
	 * seconds. Only set this value if you know none of the individual stream
	 * durations and also do not set any of them. This is deduced from the
	 * AVStream values if not set.
	 *
	 * Demuxing only, set by libavformat.
	 */
	int64_t duration;
	
	/**
	 * Total stream bitrate in bit/s, 0 if not
	 * available. Never set it directly if the file_size and the
	 * duration are known as Libav can compute it automatically.
	 */
	int bit_rate;
	
	unsigned int packet_size;
	int max_delay;
	
	/**
	 * Flags modifying the (de)muxer behaviour. A combination of AVFMT_FLAG_*.
	 * Set by the user before avformat_open_input() / avformat_write_header().
	 */
	int flags;
#define AVFMT_FLAG_GENPTS       0x0001 ///< Generate missing pts even if it requires parsing future frames.
#define AVFMT_FLAG_IGNIDX       0x0002 ///< Ignore index.
#define AVFMT_FLAG_NONBLOCK     0x0004 ///< Do not block when reading packets from input.
#define AVFMT_FLAG_IGNDTS       0x0008 ///< Ignore DTS on frames that contain both DTS & PTS
#define AVFMT_FLAG_NOFILLIN     0x0010 ///< Do not infer any values from other values, just return what is stored in the container
#define AVFMT_FLAG_NOPARSE      0x0020 ///< Do not use AVParsers, you also must set AVFMT_FLAG_NOFILLIN as the fillin code works on frames and no parsing -> no frames. Also seeking to frames can not work if parsing to find frame boundaries has been disabled
#define AVFMT_FLAG_NOBUFFER     0x0040 ///< Do not buffer frames when possible
#define AVFMT_FLAG_CUSTOM_IO    0x0080 ///< The caller has supplied a custom AVIOContext, don't avio_close() it.
#define AVFMT_FLAG_DISCARD_CORRUPT  0x0100 ///< Discard frames marked corrupted
#define AVFMT_FLAG_FLUSH_PACKETS    0x0200 ///< Flush the AVIOContext every packet.
	/**
	 * When muxing, try to avoid writing any random/volatile data to the output.
	 * This includes any random IDs, real-time timestamps/dates, muxer version, etc.
	 *
	 * This flag is mainly intended for testing.
	 */
#define AVFMT_FLAG_BITEXACT         0x0400
	
	/**
	 * Maximum size of the data read from input for determining
	 * the input container format.
	 * Demuxing only, set by the caller before avformat_open_input().
	 */
	unsigned int probesize;
	
	/**
	 * Maximum duration (in AV_TIME_BASE units) of the data read
	 * from input in avformat_find_stream_info().
	 * Demuxing only, set by the caller before avformat_find_stream_info().
	 */
	int max_analyze_duration;
	
	const uint8_t *key;
	int keylen;
	
	unsigned int nb_programs;
	AVProgram **programs;
	
	/**
	 * Forced video codec_id.
	 * Demuxing: Set by user.
	 */
	enum AVCodecID video_codec_id;
	
	/**
	 * Forced audio codec_id.
	 * Demuxing: Set by user.
	 */
	enum AVCodecID audio_codec_id;
	
	/**
	 * Forced subtitle codec_id.
	 * Demuxing: Set by user.
	 */
	enum AVCodecID subtitle_codec_id;
	
	/**
	 * Maximum amount of memory in bytes to use for the index of each stream.
	 * If the index exceeds this size, entries will be discarded as
	 * needed to maintain a smaller size. This can lead to slower or less
	 * accurate seeking (depends on demuxer).
	 * Demuxers for which a full in-memory index is mandatory will ignore
	 * this.
	 * - muxing: unused
	 * - demuxing: set by user
	 */
	unsigned int max_index_size;
	
	/**
	 * Maximum amount of memory in bytes to use for buffering frames
	 * obtained from realtime capture devices.
	 */
	unsigned int max_picture_buffer;
	
	/**
	 * Number of chapters in AVChapter array.
	 * When muxing, chapters are normally written in the file header,
	 * so nb_chapters should normally be initialized before write_header
	 * is called. Some muxers (e.g. mov and mkv) can also write chapters
	 * in the trailer.  To write chapters in the trailer, nb_chapters
	 * must be zero when write_header is called and non-zero when
	 * write_trailer is called.
	 * - muxing: set by user
	 * - demuxing: set by libavformat
	 */
	unsigned int nb_chapters;
	AVChapter **chapters;
	
	/**
	 * Metadata that applies to the whole file.
	 *
	 * - demuxing: set by libavformat in avformat_open_input()
	 * - muxing: may be set by the caller before avformat_write_header()
	 *
	 * Freed by libavformat in avformat_free_context().
	 */
	AVDictionary *metadata;
	
	/**
	 * Start time of the stream in real world time, in microseconds
	 * since the Unix epoch (00:00 1st January 1970). That is, pts=0 in the
	 * stream was captured at this real world time.
	 * Muxing only, set by the caller before avformat_write_header().
	 */
	int64_t start_time_realtime;
	
	/**
	 * The number of frames used for determining the framerate in
	 * avformat_find_stream_info().
	 * Demuxing only, set by the caller before avformat_find_stream_info().
	 */
	int fps_probe_size;
	
	/**
	 * Error recognition; higher values will detect more errors but may
	 * misdetect some more or less valid parts as errors.
	 * Demuxing only, set by the caller before avformat_open_input().
	 */
	int error_recognition;
	
	/**
	 * Custom interrupt callbacks for the I/O layer.
	 *
	 * demuxing: set by the user before avformat_open_input().
	 * muxing: set by the user before avformat_write_header()
	 * (mainly useful for AVFMT_NOFILE formats). The callback
	 * should also be passed to avio_open2() if it's used to
	 * open the file.
	 */
	AVIOInterruptCB interrupt_callback;
	
	/**
	 * Flags to enable debugging.
	 */
	int debug;
#define FF_FDEBUG_TS        0x0001
	
	/**
	 * Maximum buffering duration for interleaving.
	 *
	 * To ensure all the streams are interleaved correctly,
	 * av_interleaved_write_frame() will wait until it has at least one packet
	 * for each stream before actually writing any packets to the output file.
	 * When some streams are "sparse" (i.e. there are large gaps between
	 * successive packets), this can result in excessive buffering.
	 *
	 * This field specifies the maximum difference between the timestamps of the
	 * first and the last packet in the muxing queue, above which libavformat
	 * will output a packet regardless of whether it has queued a packet for all
	 * the streams.
	 *
	 * Muxing only, set by the caller before avformat_write_header().
	 */
	int64_t max_interleave_delta;
	
	/**
	 * Allow non-standard and experimental extension
	 * @see AVCodecContext.strict_std_compliance
	 */
	int strict_std_compliance;
	
	/**
	 * Flags for the user to detect events happening on the file. Flags must
	 * be cleared by the user once the event has been handled.
	 * A combination of AVFMT_EVENT_FLAG_*.
	 */
	int event_flags;
#define AVFMT_EVENT_FLAG_METADATA_UPDATED 0x0001 ///< The call resulted in updated metadata.
	
	/**
	 * Maximum number of packets to read while waiting for the first timestamp.
	 * Decoding only.
	 */
	int max_ts_probe;
	
	/**
	 * Avoid negative timestamps during muxing.
	 * Any value of the AVFMT_AVOID_NEG_TS_* constants.
	 * Note, this only works when using av_interleaved_write_frame.
	 * - muxing: Set by user
	 * - demuxing: unused
	 */
	int avoid_negative_ts;
#define AVFMT_AVOID_NEG_TS_AUTO             -1 ///< Enabled when required by target format
#define AVFMT_AVOID_NEG_TS_MAKE_NON_NEGATIVE 1 ///< Shift timestamps so they are non negative
#define AVFMT_AVOID_NEG_TS_MAKE_ZERO         2 ///< Shift timestamps so that they start at 0
	
	/**
	 * An opaque field for libavformat internal usage.
	 * Must not be accessed in any way by callers.
	 */
	AVFormatInternal *internal;
	
} AVFormatContext;




void ff_dynarray_add(intptr_t **tab_ptr, int *nb_ptr, intptr_t elem);

#ifdef __GNUC__
#define dynarray_add(tab, nb_ptr, elem)\
do {\
__typeof__(tab) _tab = (tab);\
__typeof__(elem) _elem = (elem);\
(void)sizeof(**_tab == _elem); /* check that types are compatible */\
ff_dynarray_add((intptr_t **)_tab, nb_ptr, (intptr_t)_elem);\
} while(0)
#else
#define dynarray_add(tab, nb_ptr, elem)\
do {\
ff_dynarray_add((intptr_t **)(tab), nb_ptr, (intptr_t)(elem));\
} while(0)
#endif




struct AVOutputFormat* av_guess_format(const char* FormatName,const char* Filename,const char* Extension);
int av_match_ext(const char* Filename,const char* Ext);
struct AVDictionaryEntry* av_dict_get(struct AVDictionary*,const char* Key,void*,int);

void* av_mallocz(size_t Size);
void* av_malloc(size_t Size);
void* av_realloc(void* Data,size_t Size);

void av_free(void*);
void av_freep(void*);
char* av_strdup(const char* s);

int64_t av_rescale(size_t Pos,uint64_t Time, uint64_t TimeBase);


void avpriv_set_pts_info(struct AVStream* Stream,int Pts,int,int Base);
int av_compare_ts(int64_t Timestamp,struct AVRational Base,int64_t Delay,int Flag);	//	AV_TIME_BASE_Q

struct AVFormatContext* avformat_alloc_context();
void avformat_free_context(struct AVFormatContext* Context);
void av_init_packet(struct AVPacket* Packet);
int avio_open_dyn_buf(struct AVIOContext** Context);	//	returns error
int ffio_free_dyn_buf(struct AVIOContext** Context);	//	returns error
size_t avio_close_dyn_buf(struct AVIOContext* Context,void* Data);
size_t avio_tell(AVIOContext* Context);
void avio_write(AVIOContext* Context,const uint8_t* Data,size_t Size);
void avio_flush(AVIOContext* Context);
int av_write_frame(struct AVFormatContext* Context,struct AVPacket* Packet);
int avformat_write_header(struct AVFormatContext* Context,void*);
struct AVStream* avformat_new_stream(struct AVFormatContext* Context,const struct AVCodec* Codec);
int avcodec_copy_context(const AVFormatContext* Source,struct AVFormatContext* Dest);

const uint8_t* avpriv_find_start_code(const uint8_t* Start,const uint8_t* End,uint32_t* State);
void assert();
void av_log(AVFormatContext *avcl, int level, const char *fmt, ...);

