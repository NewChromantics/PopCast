#include "TFileCaster.h"
#include <SoyMedia.h>
#include "SoyGif.h"
#include "THttpCaster.h"
#include <SoyFileSystem.h>
#include <SoyJson.h>


#if defined(TARGET_OSX)
#define ENABLE_LIBAV
#endif

#if defined(ENABLE_LIBAV)
#include "LibavMuxer.h"
#endif

#if defined(TARGET_OSX)
#include "AvfCompressor.h"
#include "AvfMuxer.h"
#endif

#if defined(TARGET_WINDOWS)
#include "MfMuxer.h"
#endif

#if defined(ENABLE_OPENGL)
#include <SoyOpengl.h>
#endif


std::shared_ptr<TMediaMuxer> AllocPlatformMuxer(std::string Filename,const TMediaEncoderParams& EncoderParams,std::shared_ptr<TMediaPacketBuffer>& Input,const std::function<void(bool&)>& OnStreamFinished,std::function<std::shared_ptr<TMediaEncoder>(size_t,const SoyPixelsMeta&)>& EncoderFunc)
{
#if defined(TARGET_OSX)
	const char* SupportedExtensions[] = {".mp4",".mov"};
	if ( Soy::StringEndsWith( Filename, SupportedExtensions, false ) )
	{
		if ( Soy::StringTrimLeft( Filename, "file:", false ) )
		{
			return std::make_shared<Avf::TFileMuxer>( Filename, Input, OnStreamFinished );
		}
	}
#endif
#if defined(TARGET_WINDOWS)
	const char* SupportedExtensions[] = {".mp4"};

	if ( Soy::StringEndsWith( Filename, SupportedExtensions, false ) )
	{
		if ( Soy::StringTrimLeft( Filename, "file:", false ) )
		{
			EncoderFunc = [Input](size_t StreamIndex,const SoyPixelsMeta& InputMeta)
			{
				//	get this meta from MediaFoundation::TFileMuxer
				SoyPixelsMeta OutputMeta( InputMeta.GetWidth(), InputMeta.GetHeight(), SoyPixelsFormat::BGRA );

				return std::shared_ptr<TMediaEncoder>( new MfEncoder( Input, StreamIndex, OutputMeta ) );
			};
			return std::make_shared<MediaFoundation::TFileMuxer>( Filename, EncoderParams, Input, OnStreamFinished );
		}
	}
#endif
	return nullptr;
}


std::function<std::shared_ptr<TStreamWriter>()> GetAllocStreamWriterFunc(std::string Filename)
{
	if ( Soy::StringTrimLeft( Filename, "http:", false ) )
	{
		auto f = [=]() -> std::shared_ptr<TStreamWriter>
		{
			return std::make_shared<THttpFileWriter>( Filename );
		};
		return f;
	}
	
	//	detect directory to put file in
	if ( Soy::StringTrimLeft( Filename, "file:", false ) )
	{
		auto f = [Filename]
		{
			return std::make_shared<TFileStreamWriter>( Filename );
		};
		return f;
	}

	return nullptr;
}

std::shared_ptr<TStreamWriter> AllocStreamWriter(const std::string& Filename)
{
	auto Func = GetAllocStreamWriterFunc(Filename);
	if ( Func )
	{
		return Func();
	}

	std::stringstream Error;
	Error << "Don't know what stream writer to make for " << Filename;
	throw Soy::AssertException( Error.str() );
}


std::shared_ptr<TMediaMuxer> AllocMuxer(const TCasterParams& Params,std::string Filename,std::shared_ptr<TStreamWriter> Output,std::shared_ptr<TMediaPacketBuffer> Input,std::function<std::shared_ptr<TMediaEncoder>(size_t,const SoyPixelsMeta&)>& EncoderFunc,TCasterDeviceParams& DeviceParams)
{
	if ( Soy::StringEndsWith( Filename, ".gif", false ) )
	{
		auto AllocGifEncoder = [Input,DeviceParams,Params](size_t StreamIndex,const SoyPixelsMeta& InputMeta)
		{
			if ( DeviceParams.OpenglContext )
			{
				return Gif::AllocEncoder( Input, StreamIndex, DeviceParams.OpenglContext, DeviceParams.OpenglTexturePool, Params.mGifParams, Params.mSkipFrames );
			}
#if defined(ENABLE_DIRECTX)
			else if ( DeviceParams.DirectxContext )
			{
				return Gif::AllocEncoder( Input, StreamIndex, DeviceParams.DirectxContext, DeviceParams.DirectxTexturePool, Params.mGifParams, Params.mSkipFrames );
			}
#endif

			throw Soy::AssertException("Missing graphics context");
		};
		EncoderFunc = AllocGifEncoder;
		return std::make_shared<Gif::TMuxer>( Output, Input, Filename, Params.mGifParams );
	}
	
	if ( Soy::StringEndsWith( Filename, ".raw", false ) )
	{
		return std::make_shared<TRawMuxer>( Output, Input );
	}
	
	/*
	if ( Soy::StringEndsWith( Filename, ".ts", false ) )
	{
		return std::make_shared<TMpeg2TsMuxer>( Output, Input );
	}
	*/
	
	//	libav formats
	/*
	{
		return std::make_shared<Libav::TMuxer>( Output, Input );
	}
	*/
	
	std::stringstream Error;
	Error << "Don't know what muxer to make for " << Filename;
	throw Soy::AssertException( Error.str() );
}



TFileCaster::TFileCaster(const TCasterParams& Params,TCasterDeviceParams& DeviceParams) :
	TCaster			( Params )
{
	auto& Filename = Params.mName;
	
	//	alloc & listen for new packets
	mFrameBuffer.reset( new TMediaPacketBuffer() );


	bool ShowFileOnFinished = Params.mShowFinishedFile;
	auto OnStreamFinished = [Filename,ShowFileOnFinished](bool& Success)
	{
		if ( !Success )
			return;
		if ( !ShowFileOnFinished )
			return;

		Platform::ShowFileExplorer( Filename );
	};
	
	//	see if there are OS specialisations
	mMuxer = AllocPlatformMuxer( Params.mName, Params.mMpegParams, mFrameBuffer, OnStreamFinished, mAllocEncoder );
	
	//	alloc stream & muxer from name
	if ( !mMuxer )
	{
		mFileStream = AllocStreamWriter( Params.mName );
		Soy::Assert( mFileStream != nullptr, "Failed to allocate filestream");
		mFileStream->mOnShutdown.AddListener( OnStreamFinished );
		mMuxer = AllocMuxer( Params, Filename, mFileStream, mFrameBuffer, mAllocEncoder, DeviceParams );
	}

	
	/*
#if defined(TARGET_OSX)
	static bool UseAvfMuxer = true;
#endif
	
	if ( Soy::StringEndsWith( Params.mName, ".gif", false ) )
	{
		auto AllocGifEncoder = [this,OpenglContext](size_t StreamIndex)
		{
			return Gif::AllocEncoder( mFrameBuffer, StreamIndex, OpenglContext, mParams.mGifParams );
		};
		mAllocEncoder = AllocGifEncoder;
		mFileStream.reset( new TFileStreamWriter( Filename ) );
		mMuxer.reset( new Gif::TMuxer( mFileStream, mFrameBuffer, Filename ) );
	}
#if defined(TARGET_OSX)
	else if ( UseAvfMuxer )
	{
		mMuxer.reset( new Avf::TFileMuxer( Filename, mFileStream, mFrameBuffer ) );
	}
	else
#endif
	if ( Soy::StringEndsWith( Params.mName, ".raw", false ) )
	{
		//	alloc muxer
		mFileStream.reset( new TFileStreamWriter( Filename ) );
		mMuxer.reset( new TRawMuxer( mFileStream, mFrameBuffer ) );
	}
	else
	{
		//	alloc muxer
		mFileStream.reset( new TFileStreamWriter( Filename ) );
		mMuxer.reset( new Libav::TMuxer( mFileStream, mFrameBuffer ) );
	}
	 */
	
	if ( mFileStream )
		mFileStream->Start();
}

TFileCaster::~TFileCaster()
{
	//	wait for encoder
	for ( auto& Encoder : mEncoders )
	{
		auto& pEncoder = Encoder.second;
		pEncoder.reset();
	}
	
	if ( mMuxer )
	{
		mMuxer->Finish();
		mMuxer.reset();
	}

	//	let the stream finish writing file
	if ( mFileStream )
		mFileStream->WaitForQueueToFinish();
	
	mFileStream.reset();
	
	mFrameBuffer.reset();
}

bool TFileCaster::HandlesFilename(const std::string& Filename)
{
	auto Func = GetAllocStreamWriterFunc(Filename);
	return Func != nullptr;
}

void TFileCaster::Write(const Opengl::TTexture& Image,const TCastFrameMeta& FrameMeta,Opengl::TContext& Context)
{
	auto& Encoder = AllocEncoder( FrameMeta.mStreamIndex, Image.GetMeta() );
	Encoder.Write( Image, FrameMeta.mTimecode, Context );
}

void TFileCaster::Write(const Directx::TTexture& Image,const TCastFrameMeta& FrameMeta,Directx::TContext& Context)
{
#if defined(ENABLE_DIRECTX)
	auto& Encoder = AllocEncoder( FrameMeta.mStreamIndex, Image.GetMeta() );
	Encoder.Write( Image, FrameMeta.mTimecode, Context );
#else
	throw Soy::AssertException("Directx not supported on this platform");
#endif
}

void TFileCaster::Write(std::shared_ptr<SoyPixelsImpl> Image,const TCastFrameMeta& FrameMeta)
{
	auto& Encoder = AllocEncoder( FrameMeta.mStreamIndex, Image->GetMeta() );
	Encoder.Write( Image, FrameMeta.mTimecode );
}


TMediaEncoder& TFileCaster::AllocEncoder(size_t StreamIndex,const SoyPixelsMeta& InputMeta)
{
	auto& pEncoder = mEncoders[StreamIndex];
	if ( pEncoder )
		return *pEncoder;
	
	if ( mAllocEncoder )
	{
		pEncoder = mAllocEncoder( StreamIndex, InputMeta );
		if ( pEncoder )
			return *pEncoder;
	}
	
	pEncoder.reset( new TMediaPassThroughEncoder( mFrameBuffer, StreamIndex ) );
	return *pEncoder;
}

void TFileCaster::GetMeta(TJsonWriter& Json)
{
	if ( mMuxer )
	{
		mMuxer->GetMeta( Json );
	}

	if ( mFileStream )
	{
		auto BytesWritten = mFileStream->GetBytesWritten();
		auto PendingWrites = mFileStream->GetPendingWrites();
		Json.Push("BytesWritten", BytesWritten );
		Json.Push("PendingWrites", PendingWrites );
	}

	if ( mFrameBuffer )
	{
		auto EncodedFrames = mFrameBuffer->GetPacketCount();
		Json.Push("PendingEncodedFrames", EncodedFrames);
	}

	for ( auto it = mEncoders.begin(); it != mEncoders.end();	it++ )
	{
		auto pEncoder = it->second;
		pEncoder->GetMeta( Json );
	}
}

size_t TFileCaster::GetPendingPacketCount()
{
	size_t PendingCount = 0;
	if ( mMuxer )
	{
		if ( mMuxer->mInput )
			PendingCount += mMuxer->mInput->GetPacketCount();
		PendingCount += mMuxer->mDefferedPackets.GetSize();
	}

	if ( mFileStream )
		PendingCount += mFileStream->GetPendingWrites();

	for ( auto it = mEncoders.begin(); it != mEncoders.end();	it++ )
	{
		auto pEncoder = it->second;
		PendingCount += pEncoder->GetPendingOutputCount();
		PendingCount += pEncoder->GetPendingEncodeCount();
	}

	return PendingCount;
}


/*
const uint32 AP4_FTYP_BRAND_ISOM = AP4_ATOM_TYPE('i','s','o','m');
const AP4_UI32 AP4_FTYP_BRAND_MP41 = AP4_ATOM_TYPE('m','p','4','1');
const AP4_UI32 AP4_FTYP_BRAND_MP42 = AP4_ATOM_TYPE('m','p','4','2');
const AP4_UI32 AP4_FTYP_BRAND_CCFF = AP4_ATOM_TYPE('c','c','f','f');
const AP4_UI32 AP4_FTYP_BRAND_ISML = AP4_ATOM_TYPE('i','s','m','l');
const AP4_UI32 AP4_FTYP_BRAND_PIFF = AP4_ATOM_TYPE('p','i','f','f');
const AP4_UI32 AP4_FTYP_BRAND_ISO2 = AP4_ATOM_TYPE('i','s','o','2');
const AP4_UI32 AP4_FTYP_BRAND_MSDH = AP4_ATOM_TYPE('m','s','d','h');
*/
TMultiplexerMp4::TMultiplexerMp4(TStreamBuffer& Output) :
	mOutput		( Output )
{
	//	versions from here
	//	https://github.com/EyeSee360/Bento4/blob/master/Source/C%2B%2B/Apps/Mp4Mux/Mp4Mux.cpp
/*
	mAtom.mBrand = AP4_FILE_BRAND_MP42;
	mAtom.mBrandVersion = 1;
	mAtom.mCompatbileBrands.PushBack( AP4_FILE_BRAND_ISOM );
	mAtom.mCompatbileBrands.PushBack( AP4_FILE_BRAND_MP42 );
 */
}
	
void TMultiplexerMp4::Push(TMediaPacket& Packet)
{
	//	push packet into movie
}

/*
void SoyMediaAtom_FTYP::Write(TStreamBuffer& Data)
{
	SoyMediaAtom::Write( Data );
	
	Array<char> FieldData;
	FieldData.PushBackReinterpret( mBrand );
	FieldData.PushBackReinterpret( mBrandVersion );
	for ( int i=0;	i<mCompatbileBrands.GetSize();	i++ )
		FieldData.PushBackReinterpret( mCompatbileBrands[i] );
	Data.Push( GetArrayBridge(FieldData) );
}

void TMultiplexerMp4::WriteHeader()
{
	//	write file atom
	mFileAtom.Write( mOutput );
	
	//	write other atoms except for ftyp, moov and mdat
	for ( int a=0;	a<mSubAtoms.GetSize();	a++ )
	{
		auto& Atom = mSubAtoms.GetSize();
		auto AtomType = Atom.GetType();
		if ( AtomType == AP4_ATOM_TYPE_MDAT ||
			AtomType == AP4_ATOM_TYPE_FTYP ||
			AtomType == AP4_ATOM_TYPE_MOOV )
		{
			continue;
		}
	}
	
	for ( int t=0;	t<mTracks.GetSize();	t++ )
		mTracks[t].RecalcChunkOffsets();
	
	mMoovAtom.Write( Data );
	
	mMdatAtom.Write( Data );

	for ( int t=0;	t<mTracks.GetSize();	t++ )
	{
		auto& Track = mTracks[t];

		AP4_Track*    track = track_item->GetData();
		AP4_TrakAtom* trak  = track->UseTrakAtom();
		
		// restore the backed-up chunk offsets
		result = trak->SetChunkOffsets(*trak_chunk_offsets_backup[t]);
		
		// write all the track's samples
		AP4_Cardinal   sample_count = track->GetSampleCount();
		AP4_Sample     sample;
		AP4_DataBuffer sample_data;
		for (AP4_Ordinal i=0; i<sample_count; i++) {
			track->ReadSample(i, sample, sample_data);
			stream.Write(sample_data.GetData(), sample_data.GetDataSize());
		}
	}
	

}

void SoyMediaAtom::Write(TStreamBuffer& Data)
{
	//	write header
	{
		Array<char> Header;
		uint32 Size32;
		uint64 Size64;
		uint32 Type = x;
		GetHeaderSize( SIze32, Size64 );
		Header.PushBackReinterpret( Size32 );
		Header.PushBackReinterpret( Type );
		if ( Size32 == 1 )
			Header.PushBackReinterpret( Size64 );

		// for full atoms, write version and flags
		if ( IsFullAtom() )
		{
			result = stream.WriteUI08(m_Version);
			if (AP4_FAILED(result)) return result;
			result = stream.WriteUI24(m_Flags);
			if (AP4_FAILED(result)) return result;
		}
		
		Data.Push( GetArrayBridge(Header) );
	}
}


void SoyMediaAtom::Write(TStreamBuffer& Data)
{
	//	write header
	{
		Array<char> Header;
		uint32 Size32;
		uint64 Size64;
		uint32 Type = x;
		GetHeaderSize( SIze32, Size64 );
		Header.PushBackReinterpret( Size32 );
		Header.PushBackReinterpret( Type );
		if ( Size32 == 1 )
			Header.PushBackReinterpret( Size64 );
		
		// for full atoms, write version and flags
		if ( IsFullAtom() )
		{
			result = stream.WriteUI08(m_Version);
			if (AP4_FAILED(result)) return result;
			result = stream.WriteUI24(m_Flags);
			if (AP4_FAILED(result)) return result;
		}
		
		Data.Push( GetArrayBridge(Header) );
	}
	
	{
		Array<char> FieldData;
		FieldData.PushBackReinterpret( mBrand );
		FieldData.PushBackReinterpret( mBrandVersion );
		for ( int i=0;	i<mCompatbileBrands.GetSize();	i++ )
			FieldData.PushBackReinterpret( mCompatbileBrands[i] );
		Data.Push( GetArrayBridge(FieldData) );
	}
	
	for ( int t=0;	t<mTracks.GetSize();	t++ )
		mTracks[t].RecalcChunkOffsets();
	
	mMoovAtom.Write( Data );
	
	// create and write the media data (mdat)
	// FIXME: this only supports 32-bit mdat size
	stream.WriteUI32((AP4_UI32)mdat_size);
	stream.WriteUI32(AP4_ATOM_TYPE_MDAT);
	
	// write all tracks and restore the chunk offsets to their backed-up values
	for (AP4_List<AP4_Track>::Item* track_item = movie->GetTracks().FirstItem();
		 track_item;
		 track_item = track_item->GetNext(), ++t) {
		AP4_Track*    track = track_item->GetData();
		AP4_TrakAtom* trak  = track->UseTrakAtom();
		
		// restore the backed-up chunk offsets
		result = trak->SetChunkOffsets(*trak_chunk_offsets_backup[t]);
		
		// write all the track's samples
		AP4_Cardinal   sample_count = track->GetSampleCount();
		AP4_Sample     sample;
		AP4_DataBuffer sample_data;
		for (AP4_Ordinal i=0; i<sample_count; i++) {
			track->ReadSample(i, sample, sample_data);
			stream.Write(sample_data.GetData(), sample_data.GetDataSize());
		}
	}
	
}
*/

TRawMuxer::TRawMuxer(std::shared_ptr<TStreamWriter>& Output,std::shared_ptr<TMediaPacketBuffer>& Input) :
	TMediaMuxer		( Output, Input, "TRawMuxer" ),
	mStreamIndex	( -1 )
{
	
}


void TRawMuxer::SetupStreams(const ArrayBridge<TStreamMeta>&& Streams)
{
	
}


void TRawMuxer::ProcessPacket(std::shared_ptr<TMediaPacket> Packet,TStreamWriter& Output)
{
	//	first packet!
	if ( mStreamIndex == -1 )
		mStreamIndex = size_cast<int>(Packet->mMeta.mStreamIndex);
	
	//	ignore packets from different streams
	if ( mStreamIndex != Packet->mMeta.mStreamIndex )
	{
		std::stringstream Error;
		Error << __func__ << " ignoring packet from stream " << Packet->mMeta.mStreamIndex << ", filtering only " << mStreamIndex;
		std::Debug << Error.str() << std::endl;
		return;
	}
	
	//	write out
	std::shared_ptr<Soy::TWriteProtocol> Protocol( new TRawWritePacketProtocol( Packet ) );
	Output.Push( Protocol );
}
	


