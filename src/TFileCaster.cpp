#include "TFileCaster.h"
#include <SoyMedia.h>
#include "AvfCompressor.h"


TFileCaster::TFileCaster(const TCasterParams& Params) :
	SoyWorkerThread	( std::string("TFileCaster/")+Params.mName, SoyWorkerWaitMode::Wake )
{
	auto& Filename = Params.mName;
	
	//	open file
	mFileStream = std::ofstream( Filename, std::ios::out );
	Soy::Assert( mFileStream.is_open(), std::string("Failed to open ")+Filename );
	
	//	alloc & listen for new packets
	mFrameBuffer.reset( new TMediaPacketBuffer() );
	this->WakeOnEvent( mFrameBuffer->mOnNewPacket );
	
	//	alloc encoder per stream
	size_t StreamIndex = 0;
	mEncoder.reset( new Avf::TEncoder( Params, mFrameBuffer, StreamIndex ) );
	
	//	alloc muxer
	mMulitplexer.reset( new TMultiplexerMp4( mFileData ) );
	this->WakeOnEvent( mFileData.mOnDataPushed );
	
	Start();
}

TFileCaster::~TFileCaster()
{
	//	wait for encoder
	mEncoder.reset();

	WaitToFinish();
	mFileStream.close();
	
	mFrameBuffer.reset();
}

bool TFileCaster::Iteration()
{
	//	push frames to muxer
	while ( true )
	{
		try
		{
			//	pop frames
			auto pPacket = mFrameBuffer->PopPacket();
			if ( !pPacket )
				break;

			auto& Packet = *pPacket;
			mMulitplexer->Push( Packet );
		
		}
		catch (std::exception& e)
		{
			std::Debug << "TFileCaster get-packet error: " << e.what() << std::endl;
			break;
		}
	}
	
	//	write to file
	while ( mFileData.GetBufferedSize() > 0 )
	{
		try
		{
			Array<char> Buffer;
			if ( !mFileData.Pop( mFileData.GetBufferedSize(), GetArrayBridge(Buffer) ) )
				break;
			
			if ( Buffer.IsEmpty() )
				break;
			
			//	write frame
			mFileStream.write( Buffer.GetArray(), Buffer.GetDataSize() );
			
			//Soy::Assert( File.fail(), "Error writing to file" );
			Soy::Assert( !mFileStream.bad(), "Error writing to file" );
		}
		catch (std::exception& e)
		{
			std::Debug << "TFileCaster write-file error: " << e.what() << std::endl;
			break;
		}
	}

	return true;
}


bool TFileCaster::CanSleep()
{
	if ( !mFrameBuffer )
		return true;
	
	return mFrameBuffer->HasPackets();
}

void TFileCaster::Write(const Opengl::TTexture& Image,SoyTime Timecode,Opengl::TContext& Context)
{
	Soy::Assert( mEncoder!=nullptr, "Missing encoder" );
	mEncoder->Write( Image, Timecode, Context );
}

void TFileCaster::Write(const std::shared_ptr<SoyPixelsImpl> Image,SoyTime Timecode)
{
	Soy::Assert( mEncoder!=nullptr, "Missing encoder" );
	mEncoder->Write( Image, Timecode );
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
