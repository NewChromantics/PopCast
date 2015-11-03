#include "SoyH264.h"



void ReformatDeliminator(ArrayBridge<uint8>& Data,
						 std::function<size_t(ArrayBridge<uint8>& Data,size_t Position)> ExtractChunk,
						 std::function<void(size_t ChunkLength,ArrayBridge<uint8>& Data,size_t& Position)> InsertChunk)
{
	size_t Position = 0;
	while ( true )
	{
		auto ChunkLength = ExtractChunk( Data, Position );
		if ( ChunkLength == 0 )
			break;
		{
			std::stringstream Error;
			Error << "Extracted NALU length of " << ChunkLength << "/" << Data.GetDataSize();
			Soy::Assert( ChunkLength <= Data.GetDataSize(), Error.str() );
		}
		InsertChunk( ChunkLength, Data, Position );
		Position += ChunkLength;
	}
}






void H264::ConvertToEs(SoyMediaFormat::Type& Format,ArrayBridge<uint8>&& Data)
{
	//	verify header
	Soy::Assert( Data.GetDataSize() > 4, "Missing H264 packet header" );
	Soy::Assert( Format == SoyMediaFormat::H264_8 ||
				Format == SoyMediaFormat::H264_16 ||
				Format == SoyMediaFormat::H264_32 ||
				Format == SoyMediaFormat::H264_ES
				, "Expecting a kind of H264 format input" );
	
	//	check in case it's already NALU
	uint32 Magic;
	memcpy( &Magic, Data.GetArray(), sizeof(Magic) );
	// if (nalu_size != 2 || (data[0] & 0x1F) != AP4_AVC_NAL_UNIT_TYPE_ACCESS_UNIT_DELIMITER) {
	
	if ( (Magic & 0xffffffff) == 0x00000001 )
	{
		//	check for slice info
		Format = SoyMediaFormat::H264_ES;
		return;
	}
	if ( (Magic & 0xffffff) == 0x000001 )
	{
		//	check for slice info
		Format = SoyMediaFormat::H264_ES;
		return;
	}

	//	packet can contain multiple NALU's
	//	gr: maybe split these... BUT... this packet is for a particular timecode so err... maintain multiple NALU's for a packet
	//	check for the leading size...
	int LengthSize = 0;
	if ( Format == SoyMediaFormat::H264_8 )
		LengthSize = 1;
	else if ( Format == SoyMediaFormat::H264_16 )
		LengthSize = 2;
	else if ( Format == SoyMediaFormat::H264_32 )
		LengthSize = 4;

	Soy::Assert( LengthSize != 0, "Unhandled H264 type");
	
	auto InsertChunkDelin = [](size_t ChunkLength,ArrayBridge<uint8>& Data,size_t& Position)
	{
		//	insert new delim
		uint8 Delim[] =
		{
			0, 0, 0, 1,
			9,			// NAL type = Access Unit Delimiter;
			0xF0		// Slice types = ANY
		};
		Data.InsertArray( GetRemoteArray(Delim), Position );
		Position += sizeof( Delim );
	};
	
	auto ExtractChunkDelin = [LengthSize](ArrayBridge<uint8>& Data,size_t Position)
	{
		if (Position == Data.GetDataSize() )
			return (size_t)0;
		Soy::Assert( Position < Data.GetDataSize(), "H264 ExtractChunkDelin position gone out of bounds" );
		
		size_t ChunkLength = 0;
		
		if ( LengthSize == 1 )
		{
			ChunkLength |= Data.PopAt(Position);
		}
		else if ( LengthSize == 2 )
		{
			ChunkLength |= Data.PopAt(Position) << 8;
			ChunkLength |= Data.PopAt(Position) << 0;
		}
		else if ( LengthSize == 4 )
		{
			ChunkLength |= Data.PopAt(Position) << 24;
			ChunkLength |= Data.PopAt(Position) << 16;
			ChunkLength |= Data.PopAt(Position) << 8;
			ChunkLength |= Data.PopAt(Position) << 0;
		}
		
		return ChunkLength;
	};
	
	ReformatDeliminator( Data, ExtractChunkDelin, InsertChunkDelin );

	//	finally update the format
	Format = SoyMediaFormat::H264_ES;
}












