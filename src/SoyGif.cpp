#include "SoyGif.h"
#include "TFileCaster.h"

#include "gif.h"

namespace Gif
{
	uint8	TransparentIndex = 0;
}

void Gif::EncodeGifHeader(ArrayBridge<uint8>&& Gif,SoyPixelsMeta Meta,SoyTime FrameDuration)
{
	//	write header
	char Magic[] = "GIF89a";
	Gif.PushBackArray( GetRemoteArray(Magic) );
	
	//	write Logical Screen Descriptior
	uint16 Width = Meta.GetWidth();
	uint16 Height = Meta.GetHeight();
	
	Width = 24;
	Height = 16;
	
	bool GlobalColourTableFlag = true;
	uint8 Resolution = 0;	//	3 bits
	bool SortedPalette = true;
	
	//	gr: 0 makes... 2... (+1 * 2)
	uint8 GlobalColourTableSize = 0;	//	3 bits
	
	uint8 PackedData = (GlobalColourTableFlag<<0) | (Resolution<<(0+1)) | (SortedPalette<<(0+1+3)) | (GlobalColourTableSize<<(0+1+3+1));
	uint8 AspectRatio = 0;				//	ratio of individual pixels in 1989 mattered :)
	Gif.PushBackReinterpret( Width );
	Gif.PushBackReinterpret( Height );
	Gif.PushBack( PackedData );
	Gif.PushBack( TransparentIndex );
	Gif.PushBack( AspectRatio );

	Gif.PushBackReinterpret( TRgb8(0,0,0) );
	Gif.PushBackReinterpret( TRgb8(255,255,255) );
	
	static bool EnableAnimated = false;
	
	//	delay between frames in hundredths of a secod
	uint16 Delay = FrameDuration.GetTime() / 10;
	//	write animation header
	if ( EnableAnimated && Delay != 0 )
	{
		Gif.PushBack(0x21); // extension
		Gif.PushBack(0xff); // application specific
		Gif.PushBack(11); // length 11
		char NetscapeMagic[] = "NETSCAPE2.0";
		Gif.PushBackArray( GetRemoteArray(NetscapeMagic) ); // yes, really
		Gif.PushBack(3); // 3 bytes of NETSCAPE2.0 data
		
		Gif.PushBack(1); // JUST BECAUSE
		Gif.PushBack(0); // loop infinitely (byte 0)
		Gif.PushBack(0); // loop infinitely (byte 1)
		
		Gif.PushBack(0); // block terminator
	}
}

void Gif::EncodeGif(ArrayBridge<uint8>&& Gif,const SoyPixelsImpl& Pixels,SoyTime Duration)
{
	//	test
	char Chequer[] =
	"XXXX....XXXX....XXXX...."
	"XXXX....XXXX....XXXX...."
	"XXXX....XXXX....XXXX...."
	"XXXX....XXXX....XXXX...."
	"----OOOO----OOOO----OOOO"
	"----OOOO----OOOO----OOOO"
	"----OOOO----OOOO----OOOO"
	"----OOOO----OOOO----OOOO"
	"XXXX....XXXX....XXXX...."
	"XXXX....XXXX....XXXX...."
	"XXXX....XXXX....XXXX...."
	"XXXX....XXXX....XXXX...."
	"----OOOO----OOOO----OOOO"
	"----OOOO----OOOO----OOOO"
	"----OOOO----OOOO----OOOO"
	"----OOOO----OOOO----OOOO";
	auto ChequerArray = GetRemoteArray( Chequer );

	BufferArray<TRgb8,256> Palette(256);
	Palette.SetAll( TRgb8(0,0,0) );
	Palette['X'] = TRgb8(255,0,0);
	Palette['.'] = TRgb8(0,255,0);
	Palette['O'] = TRgb8(0,0,255);
	Palette['-'] = TRgb8(255,255,0);
	
	SoyPixels PalettisedPixels;
	PalettisedPixels.Init( 24, 16, SoyPixelsFormat::Greyscale );
	PalettisedPixels.GetPixelsArray().Copy( ChequerArray );

	EncodeGif( Gif, PalettisedPixels, GetArrayBridge(Palette), Duration );
}

// The LZW dictionary is a 256-ary tree constructed as the file is encoded,
// this is one node
class GifLzwNode
{
public:
	GifLzwNode()
	{
		memset( m_next, 0, sizeof(m_next) );
	}
			   
	uint16_t m_next[256];
};
			   
// Simple structure to write out the LZW-compressed portion of the image
// one bit at a time
struct GifBitStatus
{
	uint8_t bitIndex;  // how many bits in the partial byte written so far
	uint8_t byte;      // current partial byte
	
	uint32_t chunkIndex;
	uint8_t chunk[256];   // bytes are written in here until we have 256 of them, then written to the file
};

// insert a single bit
void GifWriteBit( GifBitStatus& stat, uint32_t bit )
{
	bit = bit & 1;
	bit = bit << stat.bitIndex;
	stat.byte |= bit;
	
	++stat.bitIndex;
	if( stat.bitIndex > 7 )
	{
		// move the newly-finished byte to the chunk buffer
		stat.chunk[stat.chunkIndex++] = stat.byte;
		// and start a new byte
		stat.bitIndex = 0;
		stat.byte = 0;
	}
}

// write all bytes so far to the file
void GifWriteChunk(ArrayBridge<uint8>& f, GifBitStatus& stat )
{
	f.PushBack( stat.chunkIndex );
	for ( int i=0;	i<stat.chunkIndex;	i++ )
		f.PushBack( stat.chunk[i] );
	
	stat.bitIndex = 0;
	stat.byte = 0;
	stat.chunkIndex = 0;
}

void GifWriteCode(ArrayBridge<uint8>& f, GifBitStatus& stat, uint32_t code, uint32_t length )
{
	for( uint32_t ii=0; ii<length; ++ii )
	{
		GifWriteBit(stat, code);
		code = code >> 1;
		
		if( stat.chunkIndex == 255 )
		{
			GifWriteChunk(f, stat);
		}
	}
}

void EncodeLzf(ArrayBridge<uint8>&& Lzf,const SoyPixelsImpl& PalettisedPixels,uint8 PaletteBitDepth)
{
	const uint8 minCodeSize = PaletteBitDepth;
	const uint32_t clearCode = 1 << PaletteBitDepth;
	
	Lzf.PushBack( minCodeSize ); // min code size 8 bits
	
	Array<GifLzwNode> codetree( size_cast<size_t>(4096) );

	int32_t curCode = -1;
	uint32_t codeSize = minCodeSize+1;
	uint32_t maxCode = clearCode+1;
	
	GifBitStatus stat;
	stat.byte = 0;
	stat.bitIndex = 0;
	stat.chunkIndex = 0;
	
	GifWriteCode(Lzf, stat, clearCode, codeSize);  // start with a fresh LZW dictionary
	
	auto width = PalettisedPixels.GetWidth();
	auto height = PalettisedPixels.GetHeight();
	
	static bool EnableCompression = false;
	
	for(uint32_t yy=0; yy<height; ++yy)
	{
		for(uint32_t xx=0; xx<width; ++xx)
		{
			//	alpha = index here
			//uint8_t nextValue = image[(yy*width+xx)*4+3];
			uint8 nextValue = PalettisedPixels.GetPixel( xx, yy, 0 );
			
			if ( !EnableCompression )
			{
				// "loser mode" - no compression, every single code is followed immediately by a clear
				GifWriteCode( Lzf, stat, nextValue, codeSize );
				GifWriteCode( Lzf, stat, 256, codeSize );
				continue;
			}
			
			
			if( curCode < 0 )
			{
				// first value in a new run
				curCode = nextValue;
			}
			else if( codetree[curCode].m_next[nextValue] )
			{
				// current run already in the dictionary
				curCode = codetree[curCode].m_next[nextValue];
			}
			else
			{
				// finish the current run, write a code
				GifWriteCode( Lzf, stat, curCode, codeSize );
				
				// insert the new run into the dictionary
				codetree[curCode].m_next[nextValue] = ++maxCode;
				
				if( maxCode >= (1ul << codeSize) )
				{
					// dictionary entry count has broken a size barrier,
					// we need more bits for codes
					codeSize++;
				}
				if( maxCode == 4095 )
				{
					// the dictionary is full, clear it out and begin anew
					GifWriteCode( Lzf, stat, clearCode, codeSize); // clear tree
					
					codetree.Clear();
					codetree.SetSize(4096);
					curCode = -1;
					codeSize = minCodeSize+1;
					maxCode = clearCode+1;
				}
				
				curCode = nextValue;
			}
		}
	}
	
	// compression footer
	GifWriteCode( Lzf, stat, curCode, codeSize );
	GifWriteCode( Lzf, stat, clearCode, codeSize );
	GifWriteCode( Lzf, stat, clearCode+1, minCodeSize+1 );
	
	// write out the last partial chunk
	while( stat.bitIndex )
		GifWriteBit(stat, 0);
	if( stat.chunkIndex )
		GifWriteChunk( Lzf, stat);
}


void Gif::EncodeGif(ArrayBridge<uint8>& Gif,const SoyPixelsImpl& PalettisedPixels,const ArrayBridge<TRgb8>&& Palette,SoyTime FrameDuration)
{
	uint8 PaletteBitDepth = 8;
	uint16 Delay = FrameDuration.GetTime() / 10;

	//	todo: write transparent pixels if same colour as prev frame
	//	expecting greyscale palette
	Soy::Assert( PalettisedPixels.GetFormat() == SoyPixelsFormat::Greyscale, "Expected paletised pixels");
	
	// graphics control extension
	Gif.PushBack(0x21);
	Gif.PushBack(0xf9);
	Gif.PushBack(0x04);
	Gif.PushBack(0x05); // leave prev frame in place, this frame has transparency
	Gif.PushBackReinterpret(Delay);
	Gif.PushBack(TransparentIndex); // transparent color index
	Gif.PushBack(0);
	
	Gif.PushBack(0x2c); // image descriptor block
	
	uint16 Left = 0;
	uint16 Top = 0;
	uint16 Width = PalettisedPixels.GetWidth();
	uint16 Height = PalettisedPixels.GetHeight();
	
	Gif.PushBackReinterpret( Left );           // corner of image in canvas space
	Gif.PushBackReinterpret( Top );
	
	Gif.PushBackReinterpret(Width);          // width and height of image
	Gif.PushBackReinterpret(Height);
	
	//Gif.PushBack(0); // no local color table, no transparency
	//Gif.PushBack(0x80); // no local color table, but transparency
	
	Gif.PushBack(0x80 + PaletteBitDepth-1); // local color table present, 2 ^ bitDepth entries
	
	//	write palette which we can do in a block
	auto Palette8 = GetRemoteArray( reinterpret_cast<const uint8*>(Palette.GetArray()), Palette.GetDataSize() );
	Gif.PushBackArray( Palette8 );
	
	//	get compressed data
	Array<uint8> LzfData;
	EncodeLzf( GetArrayBridge(LzfData), PalettisedPixels, PaletteBitDepth );

	Gif.PushBackArray( LzfData );
	
	// image block terminator
	Gif.PushBack(0);
}


void Gif::EncodeGifFooter(ArrayBridge<uint8>&& Gif)
{
	Gif.PushBack( 0x3b );
}


std::shared_ptr<TMediaEncoder> Gif::AllocEncoder(std::shared_ptr<TMediaPacketBuffer>& OutputBuffer,size_t StreamIndex)
{
	std::shared_ptr<TMediaEncoder> Encoder( new TEncoder( OutputBuffer, StreamIndex ) );
	return Encoder;
}



Gif::TMuxer::TMuxer(std::shared_ptr<TStreamWriter>& Output,std::shared_ptr<TMediaPacketBuffer>& Input,const std::string& ThreadName) :
	TMediaMuxer		( Output, Input, ThreadName )
{
}

void Gif::TMuxer::Finish()
{
	//	write gif tail byte
	std::shared_ptr<Soy::TWriteProtocol> Write( new TRawWriteDataProtocol );
	auto& Data = dynamic_cast<TRawWriteDataProtocol&>( *Write ).mData;
	auto DataBridge = GetArrayBridge(Data);
	auto& CastData = *reinterpret_cast<ArrayInterface<uint8>*>( &DataBridge );
	Gif::EncodeGifFooter( GetArrayBridge(CastData) );
	mOutput->Push( Write );
}

void Gif::TMuxer::SetupStreams(const ArrayBridge<TStreamMeta>&& Streams)
{
	Soy::Assert( Streams.GetSize() == 1, "Gif must have one stream only" );

	//	write out header
	std::shared_ptr<Soy::TWriteProtocol> Write( new TRawWriteDataProtocol );
	auto& Data = dynamic_cast<TRawWriteDataProtocol&>( *Write ).mData;
	auto DataBridge = GetArrayBridge(Data);
	auto& CastData = *reinterpret_cast<ArrayInterface<uint8>*>( &DataBridge );
	SoyTime FrameDuration( 33ull );
	Gif::EncodeGifHeader( GetArrayBridge(CastData), Streams[0].mPixelMeta, FrameDuration );
	mOutput->Push( Write );
}

void Gif::TMuxer::ProcessPacket(std::shared_ptr<TMediaPacket> Packet,TStreamWriter& Output)
{
	//	write immediately if data is gif
	Soy::Assert( Packet->mMeta.mCodec == SoyMediaFormat::GifFrame, "Gif muxer can only eat gif frame data");

	std::shared_ptr<TRawWritePacketProtocol> Protocol( new TRawWritePacketProtocol( Packet ) );
	Output.Push( Protocol );
}



Gif::TEncoder::TEncoder(std::shared_ptr<TMediaPacketBuffer>& OutputBuffer,size_t StreamIndex) :
	SoyWorkerThread	( "Gif::TEncoder", SoyWorkerWaitMode::Wake ),
	TMediaEncoder	( OutputBuffer ),
	mStreamIndex	( StreamIndex )
{
	Start();
}
	
void Gif::TEncoder::Write(const Opengl::TTexture& Image,SoyTime Timecode,Opengl::TContext& Context)
{
	//	do here in opengl...
	//		determine palette for frame (or predefined palette)
	//		reduce image to 256 colours (with dither/closest shader)
	//		read pixels
	std::shared_ptr<TMediaPacket> pPacket( new TMediaPacket );
	auto& Packet = *pPacket;
	
	//	construct pixels against data and read it
	SoyPixelsDef<Array<uint8>> Pixels( Packet.mData, Packet.mMeta.mPixelMeta );
	Image.Read( Pixels );
	
	Packet.mTimecode = Timecode;
	Packet.mMeta.mCodec = SoyMediaFormat::FromPixelFormat( Packet.mMeta.mPixelMeta.GetFormat() );

	PushFrame( pPacket );
	
	//	do on thread
	//		encode to gif format
	//		queue packet
}

void Gif::TEncoder::Write(const std::shared_ptr<SoyPixelsImpl> Image,SoyTime Timecode)
{
	throw Soy::AssertException("Currently only supporting opengl");
}

bool Gif::TEncoder::CanSleep()
{
	return mPendingFrames.GetSize() == 0;
}

bool Gif::TEncoder::Iteration()
{
	auto pPacket = PopFrame();
	if ( !pPacket )
		return true;
	auto& Packet = *pPacket;
	
	//	encode to gif format & write
	std::shared_ptr<TMediaPacket> pGifPacket( new TMediaPacket );
	pGifPacket->mMeta = Packet.mMeta;
	pGifPacket->mMeta.mCodec = SoyMediaFormat::GifFrame;
	
	SoyPixelsRemote Pixels( GetArrayBridge( Packet.mData ), Packet.mMeta.mPixelMeta );
	EncodeGif( GetArrayBridge( pGifPacket->mData ), Pixels, Packet.mDuration );
	
	//	drop packets
	auto Block = []
	{
		return false;
	};
	
	//	only one packet!
	if ( mPacketsWritten == 0 )
		TMediaEncoder::PushFrame( pGifPacket, Block );
	
	mPacketsWritten++;
	
	return true;
}


void Gif::TEncoder::PushFrame(std::shared_ptr<TMediaPacket> Frame)
{
	std::lock_guard<std::mutex> Lock( mPendingFramesLock );
	mPendingFrames.PushBack( Frame );
	Wake();
}


std::shared_ptr<TMediaPacket> Gif::TEncoder::PopFrame()
{
	if ( mPendingFrames.IsEmpty() )
		return nullptr;
	
	std::lock_guard<std::mutex> Lock( mPendingFramesLock );
	return mPendingFrames.PopAt(0);
}

