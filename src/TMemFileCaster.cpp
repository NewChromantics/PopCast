#include "TMemFileCaster.h"


const char* TMemFileHeader::Magic = "PopMovie"; //	64 bit
const uint64 TMemFileHeader::Version = 0xabcdef; //	64 bit




TMemFileHeader::TMemFileHeader(const SoyPixelsMeta& Meta,SoyTime Time) :
	mVersion		( Version ),
	mDataSize		( size_cast<uint32>(Meta.GetDataSize()) ),
	mPixelFormat	( static_cast<uint32>( Meta.GetFormat() ) ),
	mWidth			( Meta.GetWidth() ),
	mHeight			( Meta.GetHeight() ),
	mTimecode		( Time.GetTime() ),
	mError			{ 0 }
{
	memcpy( &mMagic, Magic, sizeof(mMagic) );
}


void TMemFileHeader::Verify()
{
	bool MagicOkay = 0==memcmp( &mMagic, Magic, sizeof(mMagic) );
	bool VersionOkay = 0==memcmp( &mVersion, &Version, sizeof(mVersion) );
	Soy::Assert( MagicOkay, "Memfile header magic number invalid");
	Soy::Assert( VersionOkay, "Memfile header version number invalid");
}

void TMemFileHeader::SetError(const std::string& Error)
{
	Soy::StringToBuffer( Error.c_str(), mError );
}





TMemFileCaster::TMemFileCaster(const TCasterParams& Params) :
	mFilename	( Params.mName )
{
}

void TMemFileCaster::Write(const Opengl::TTexture& Image,SoyTime Timecode)
{
	throw Soy::AssertException("todo");
}

void TMemFileCaster::Write(const SoyPixelsImpl& Image,SoyTime Timecode)
{
	//	make sure file is allocated
	AllocateFile( mFilename, Image.GetMeta() );
	
	std::shared_ptr<SoyMemFile> MemFile = mMemFile;
	Soy::Assert( MemFile != nullptr, "Memfile expected");

	//	extract rgba data
	auto& Pixels = Image.GetPixelsArray();

	//	access data
	auto HeaderArray = GetArrayBridge( MemFile->GetArray() ).GetSubArray<TMemFileHeader>( 0, 1 );
	auto& Header = HeaderArray[0];
	//	validate header
	Header.Verify();
	
	auto ColourArray = GetArrayBridge( MemFile->GetArray() ).GetSubArray<uint8>( sizeof(TMemFileHeader), Pixels.GetDataSize() );

	//	write new header
	Header = TMemFileHeader( Image.GetMeta(), Timecode );
	
	//	write new data
	memcpy( ColourArray.GetArray(), Pixels.GetArray(), ColourArray.GetDataSize() );

	Header.SetError( std::string() );
}

void TMemFileCaster::AllocateFile(const std::string& Filename,const SoyPixelsMeta& Meta)
{
	//	if already allocated, see if we need to change (and shrink if possible)
	if ( mMemFile )
	{
		if ( mAllocatedMeta == Meta )
			return;
		
		throw Soy::AssertException("Todo; check if we can re-allocate memfile with new meta");
	}

	Soy::Assert( Meta.IsValid(), "Need valid meta" );
	
	//	work out how much we need
	TMemFileHeader Header( Meta );
	size_t TotalAllocSize = sizeof(TMemFileHeader) + Header.mDataSize;
	
	//	allocate
	mMemFile.reset( new SoyMemFile( Filename, MemFileAccess::Create, TotalAllocSize ) );
}
