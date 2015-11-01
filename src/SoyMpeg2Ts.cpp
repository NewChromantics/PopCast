#include "SoyMpeg2Ts.h"


namespace Mpeg2Ts
{
	uint32	ComputeCRC(const unsigned char* data, unsigned int data_size);
}

//	gr: can;t find official documentation on whether this is enforced...
//	https://en.wikipedia.org/wiki/Packetized_elementary_stream#cite_note-7
const uint16 AP4_MPEG2_TS_DEFAULT_PID_PMT            = 0x100;
const uint16 AP4_MPEG2_TS_DEFAULT_PID_AUDIO          = 0x101;
const uint16 AP4_MPEG2_TS_DEFAULT_PID_VIDEO          = 0x102;
const uint16 AP4_MPEG2_TS_DEFAULT_STREAM_ID_AUDIO    = 0xc0;
const uint16 AP4_MPEG2_TS_DEFAULT_STREAM_ID_VIDEO    = 0xe0;
const uint16 AP4_MPEG2_TS_STREAM_ID_PRIVATE_STREAM_1 = 0xbd;

const unsigned int AP4_MPEG2TS_PACKET_SIZE         = 188;
const unsigned int AP4_MPEG2TS_PACKET_PAYLOAD_SIZE = 184;
const unsigned int AP4_MPEG2TS_SYNC_BYTE           = 0x47;
const unsigned int AP4_MPEG2TS_PCR_ADAPTATION_SIZE = 6;
const uint8 AP4_MPEG2_STREAM_TYPE_ISO_IEC_13818_1_PES = 0x06;
const uint8 AP4_MPEG2_STREAM_TYPE_ISO_IEC_13818_7     = 0x0F;
const uint8 AP4_MPEG2_STREAM_TYPE_AVC                 = 0x1B;
const uint8 AP4_MPEG2_STREAM_TYPE_HEVC                = 0x24;
const uint8 AP4_MPEG2_STREAM_TYPE_ATSC_AC3            = 0x81;


static const uint32 CRC_Table[256] = {
	0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9, 0x130476dc, 0x17c56b6b,
	0x1a864db2, 0x1e475005, 0x2608edb8, 0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61,
	0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd, 0x4c11db70, 0x48d0c6c7,
	0x4593e01e, 0x4152fda9, 0x5f15adac, 0x5bd4b01b, 0x569796c2, 0x52568b75,
	0x6a1936c8, 0x6ed82b7f, 0x639b0da6, 0x675a1011, 0x791d4014, 0x7ddc5da3,
	0x709f7b7a, 0x745e66cd, 0x9823b6e0, 0x9ce2ab57, 0x91a18d8e, 0x95609039,
	0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5, 0xbe2b5b58, 0xbaea46ef,
	0xb7a96036, 0xb3687d81, 0xad2f2d84, 0xa9ee3033, 0xa4ad16ea, 0xa06c0b5d,
	0xd4326d90, 0xd0f37027, 0xddb056fe, 0xd9714b49, 0xc7361b4c, 0xc3f706fb,
	0xceb42022, 0xca753d95, 0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1,
	0xe13ef6f4, 0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d, 0x34867077, 0x30476dc0,
	0x3d044b19, 0x39c556ae, 0x278206ab, 0x23431b1c, 0x2e003dc5, 0x2ac12072,
	0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16, 0x018aeb13, 0x054bf6a4,
	0x0808d07d, 0x0cc9cdca, 0x7897ab07, 0x7c56b6b0, 0x71159069, 0x75d48dde,
	0x6b93dddb, 0x6f52c06c, 0x6211e6b5, 0x66d0fb02, 0x5e9f46bf, 0x5a5e5b08,
	0x571d7dd1, 0x53dc6066, 0x4d9b3063, 0x495a2dd4, 0x44190b0d, 0x40d816ba,
	0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e, 0xbfa1b04b, 0xbb60adfc,
	0xb6238b25, 0xb2e29692, 0x8aad2b2f, 0x8e6c3698, 0x832f1041, 0x87ee0df6,
	0x99a95df3, 0x9d684044, 0x902b669d, 0x94ea7b2a, 0xe0b41de7, 0xe4750050,
	0xe9362689, 0xedf73b3e, 0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2,
	0xc6bcf05f, 0xc27dede8, 0xcf3ecb31, 0xcbffd686, 0xd5b88683, 0xd1799b34,
	0xdc3abded, 0xd8fba05a, 0x690ce0ee, 0x6dcdfd59, 0x608edb80, 0x644fc637,
	0x7a089632, 0x7ec98b85, 0x738aad5c, 0x774bb0eb, 0x4f040d56, 0x4bc510e1,
	0x46863638, 0x42472b8f, 0x5c007b8a, 0x58c1663d, 0x558240e4, 0x51435d53,
	0x251d3b9e, 0x21dc2629, 0x2c9f00f0, 0x285e1d47, 0x36194d42, 0x32d850f5,
	0x3f9b762c, 0x3b5a6b9b, 0x0315d626, 0x07d4cb91, 0x0a97ed48, 0x0e56f0ff,
	0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623, 0xf12f560e, 0xf5ee4bb9,
	0xf8ad6d60, 0xfc6c70d7, 0xe22b20d2, 0xe6ea3d65, 0xeba91bbc, 0xef68060b,
	0xd727bbb6, 0xd3e6a601, 0xdea580d8, 0xda649d6f, 0xc423cd6a, 0xc0e2d0dd,
	0xcda1f604, 0xc960ebb3, 0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7,
	0xae3afba2, 0xaafbe615, 0xa7b8c0cc, 0xa379dd7b, 0x9b3660c6, 0x9ff77d71,
	0x92b45ba8, 0x9675461f, 0x8832161a, 0x8cf30bad, 0x81b02d74, 0x857130c3,
	0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640, 0x4e8ee645, 0x4a4ffbf2,
	0x470cdd2b, 0x43cdc09c, 0x7b827d21, 0x7f436096, 0x7200464f, 0x76c15bf8,
	0x68860bfd, 0x6c47164a, 0x61043093, 0x65c52d24, 0x119b4be9, 0x155a565e,
	0x18197087, 0x1cd86d30, 0x029f3d35, 0x065e2082, 0x0b1d065b, 0x0fdc1bec,
	0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088, 0x2497d08d, 0x2056cd3a,
	0x2d15ebe3, 0x29d4f654, 0xc5a92679, 0xc1683bce, 0xcc2b1d17, 0xc8ea00a0,
	0xd6ad50a5, 0xd26c4d12, 0xdf2f6bcb, 0xdbee767c, 0xe3a1cbc1, 0xe760d676,
	0xea23f0af, 0xeee2ed18, 0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4,
	0x89b8fd09, 0x8d79e0be, 0x803ac667, 0x84fbdbd0, 0x9abc8bd5, 0x9e7d9662,
	0x933eb0bb, 0x97ffad0c, 0xafb010b1, 0xab710d06, 0xa6322bdf, 0xa2f33668,
	0xbcb4666d, 0xb8757bda, 0xb5365d03, 0xb1f740b4
};


uint32 Mpeg2Ts::ComputeCRC(const unsigned char* data, unsigned int data_size)
{
	uint32 crc = 0xFFFFFFFF;
	
	for (unsigned int i=0; i<data_size; i++) {
		crc = (crc << 8) ^ CRC_Table[((crc >> 24) ^ *data++) & 0xFF];
	}
	
	return crc;
}

class TArrayBitWriter	//	AP4_BitWriter
{
public:
	TArrayBitWriter(ArrayBridge<uint8>&& Array) :
		mData		( Array ),
		mBitCount	( 0 )
	{
	}
	
	void Write(uint32 bits,size_t bit_count);
	
private:
	ArrayBridge<uint8>&	mData;
	size_t				mBitCount;
};



void TArrayBitWriter::Write(uint32 bits,size_t bit_count)
{
	//	onto next byte
	if ( mBitCount+bit_count > mData.GetDataSize()* 8 )
	{
		mData.PushBack(0);
	}

	auto* data = mData.GetArray();
	data += mBitCount/8;
	unsigned int space = 8-(mBitCount%8);
	while (bit_count)
	{
		unsigned int mask = bit_count==32 ? 0xFFFFFFFF : ((1<<bit_count)-1);
		if (bit_count <= space)
		{
			*data |= ((bits&mask) << (space-bit_count));
			mBitCount += bit_count;
			return;
		}
		else
		{
			*data |= ((bits&mask) >> (bit_count-space));
			++data;
			mBitCount += space;
			bit_count  -= space;
			space       = 8;
		}
	}
}


Mpeg2Ts::TPacket::TPacket(const TStreamMeta& Stream,size_t PacketCounter) :
	mStreamMeta					( Stream ),
	mPacketContinuityCounter	( PacketCounter )
{
}

void Mpeg2Ts::TPacket::WriteHeader(bool PayloadStart,size_t PayloadSize,bool WithPcr,TStreamBuffer& Buffer)
{
	uint16 Pid = mStreamMeta.mProgramId;
	BufferArray<uint8,4> header(4);
	header[0] = AP4_MPEG2TS_SYNC_BYTE;
	header[1] = (uint8)(((PayloadStart?1:0)<<6) | (Pid >> 8));
	header[2] = Pid & 0xFF;
	
	unsigned int adaptation_field_size = 0;
	if (WithPcr)
		adaptation_field_size += 2+AP4_MPEG2TS_PCR_ADAPTATION_SIZE;
	
	// clamp the payload size
	if (PayloadSize+adaptation_field_size > AP4_MPEG2TS_PACKET_PAYLOAD_SIZE) {
		PayloadSize = AP4_MPEG2TS_PACKET_PAYLOAD_SIZE-adaptation_field_size;
	}
	
	// adjust the adaptation field to include stuffing if necessary
	if (adaptation_field_size+PayloadSize < AP4_MPEG2TS_PACKET_PAYLOAD_SIZE) {
		adaptation_field_size = AP4_MPEG2TS_PACKET_PAYLOAD_SIZE - size_cast<unsigned int>(PayloadSize);
	}
	
	if (adaptation_field_size == 0) {
		// no adaptation field
		header[3] = (1<<4) | ((mPacketContinuityCounter)&0x0F);
		Buffer.Push( GetArrayBridge(header) );
	} else {
		// adaptation field present
		header[3] = (3<<4) | ((mPacketContinuityCounter)&0x0F);
		Buffer.Push( GetArrayBridge(header) );
		
		Array<uint8> Data;
		
		if (adaptation_field_size == 1) {
			// just one byte (stuffing)
			Data.PushBack(0);
		} else {
			// two or more bytes (stuffing and/or PCR)
			Data.PushBack( adaptation_field_size-1 );
			Data.PushBack( WithPcr ? (1<<4) : 0 );
			unsigned int pcr_size = 0;
			if (WithPcr)
			{
				uint64 Pcr = 0;	//	provide in params
				pcr_size = AP4_MPEG2TS_PCR_ADAPTATION_SIZE;
				uint64 pcr_base = Pcr/300;
				uint32 pcr_ext  = size_cast<uint32>( Pcr%300 );
				Soy_AssertTodo();
				/*
				AP4_BitWriter writer(pcr_size);
				writer.Write((AP4_UI32)(pcr_base>>32), 1);
				writer.Write((AP4_UI32)pcr_base, 32);
				writer.Write(0x3F, 6);
				writer.Write(pcr_ext, 9);
				output.Write(writer.GetData(), pcr_size);
				 */
			}
			if (adaptation_field_size > 2)
			{
				for ( int i=0;	i<adaptation_field_size-pcr_size-2;	i++ )
				{
					Data.PushBack(0xff);
				}
			}
		}
		
		Buffer.Push( GetArrayBridge(Data) );
	}
}


Mpeg2Ts::TPesPacket::TPesPacket(std::shared_ptr<TMediaPacket> Packet,const TStreamMeta& Stream,size_t PacketCounter) :
	mPacket						( Packet ),
	TPacket						( Stream, PacketCounter )
{
}


void Mpeg2Ts::TPesPacket::Encode(TStreamBuffer& Buffer)
{
	//	write sample
}


Mpeg2Ts::TPatPacket::TPatPacket(uint16 ProgramId) :
	TPacket		( TStreamMeta(0,0,ProgramId), 0 )
{
	
}

void Mpeg2Ts::TPatPacket::Encode(TStreamBuffer& Buffer)
{
	bool PayloadStart = true;
	size_t PayloadSize = AP4_MPEG2TS_PACKET_PAYLOAD_SIZE;
	bool WithPcr = false;
	WriteHeader( PayloadStart, PayloadSize, WithPcr, Buffer );
	
	//	was 1024... but only end up writing 17...
	BufferArray<uint8,17> Payload(17);
	TArrayBitWriter writer( GetArrayBridge(Payload) );
	
	writer.Write(0, 8);  // pointer
	writer.Write(0, 8);  // table_id
	writer.Write(1, 1);  // section_syntax_indicator
	writer.Write(0, 1);  // '0'
	writer.Write(3, 2);  // reserved
	writer.Write(13, 12);// section_length
	writer.Write(1, 16); // transport_stream_id
	writer.Write(3, 2);  // reserved
	writer.Write(0, 5);  // version_number
	writer.Write(1, 1);  // current_next_indicator
	writer.Write(0, 8);  // section_number
	writer.Write(0, 8);  // last_section_number
	writer.Write(1, 16); // program number
	writer.Write(7, 3);  // reserved
	writer.Write( mStreamMeta.mProgramId, 13); // program_map_PID
	
	auto Crc = ComputeCRC( &Payload[1], 17-1-4 );
	writer.Write( Crc, 32 );
	//writer.Write(ComputeCRC(writer.GetData()+1, 17-1-4), 32);
	
	Buffer.Push( GetArrayBridge(Payload) );

	Array<uint8> Padding;
	Padding.SetSize( AP4_MPEG2TS_PACKET_PAYLOAD_SIZE-Payload.GetDataSize() );
	Padding.SetAll( 0xff );
	Buffer.Push( GetArrayBridge(Padding) );

}

Mpeg2Ts::TPmtPacket::TPmtPacket(const std::map<size_t,Mpeg2Ts::TStreamMeta>& Streams,uint16 ProgramId) :
	TPacket	( TStreamMeta(0,0,ProgramId), 0 )
{
	Array<uint8> Payload;
	TArrayBitWriter writer( GetArrayBridge(Payload) );
	
	unsigned int section_length = 13;
	unsigned int pcr_pid = mStreamMeta.mProgramId;

	for ( auto it=Streams.begin();	it!=Streams.end();	it++ )
	{
		auto& Stream = it->second;
		Soy::Assert( Stream.mProgramId == pcr_pid, "Stream has mis-matched program id... skip, or error?" );
		
		section_length += 5+Stream.mDescriptor.GetDataSize();
	}

	writer.Write(0, 8);        // pointer
	writer.Write(2, 8);        // table_id
	writer.Write(1, 1);        // section_syntax_indicator
	writer.Write(0, 1);        // '0'
	writer.Write(3, 2);        // reserved
	writer.Write(section_length, 12); // section_length
	writer.Write(1, 16);       // program_number
	writer.Write(3, 2);        // reserved
	writer.Write(0, 5);        // version_number
	writer.Write(1, 1);        // current_next_indicator
	writer.Write(0, 8);        // section_number
	writer.Write(0, 8);        // last_section_number
	writer.Write(7, 3);        // reserved
	writer.Write(pcr_pid, 13); // PCD_PID
	writer.Write(0xF, 4);      // reserved
	writer.Write(0, 12);       // program_info_length

	for ( auto it=Streams.begin();	it!=Streams.end();	it++ )
	{
		auto& Stream = it->second;

		writer.Write( Stream.mStreamType, 8);                // stream_type
		writer.Write(0x7, 3);                                  // reserved
		writer.Write( Stream.mProgramId, 13);                   // elementary_PID
		writer.Write(0xF, 4);                                  // reserved
		writer.Write( Stream.mDescriptor.GetDataSize(), 12); // ES_info_length

		for ( int i=0;	i<Stream.mDescriptor.GetDataSize(); i++ )
		{
			writer.Write( Stream.mDescriptor[i], 8 );
		}
	}

	auto Crc = ComputeCRC( &Payload[1], section_length-1);
	writer.Write(Crc, 32); // CRC

	Payload.SetSize( section_length+4 );
	mPayload.PushBackArray( Payload );
	
	for ( int i=0;	i<AP4_MPEG2TS_PACKET_PAYLOAD_SIZE-(section_length+4);	i++ )
	{
		mPayload.PushBack(0xff);
	}
}

void Mpeg2Ts::TPmtPacket::Encode(TStreamBuffer& Buffer)
{
	WriteHeader( true, AP4_MPEG2TS_PACKET_PAYLOAD_SIZE, false, Buffer );
	Buffer.Push( GetArrayBridge( mPayload ) );
}

TMpeg2TsMuxer::TMpeg2TsMuxer(std::shared_ptr<TStreamWriter>& Output,std::shared_ptr<TMediaPacketBuffer>& Input) :
	SoyWorkerThread	( "TMpeg2TsMuxer", SoyWorkerWaitMode::Wake ),
	mPacketCounter	( 0 ),
	mProgramId		( AP4_MPEG2_TS_DEFAULT_PID_PMT ),
	mOutput			( Output ),
	mInput			( Input )
{
	Soy::Assert( mOutput!=nullptr, "TMpeg2TsMuxer output missing");
	Soy::Assert( mInput!=nullptr, "TMpeg2TsMuxer input missing");
	mOnPacketListener = WakeOnEvent( mInput->mOnNewPacket );
	
	//	http://wiki.multimedia.cx/index.php?title=MPEG-2_Transport_Stream#PAT
	
	Start();
}


bool TMpeg2TsMuxer::CanSleep()
{
	if ( !mInput )
		return true;
	if ( !mInput->HasPackets() )
		return true;
	
	return false;
}

bool TMpeg2TsMuxer::Iteration()
{
	//	pop next packet
	if ( !mInput )
		return true;
	
	auto Packet = mInput->PopPacket();
	if ( !Packet )
		return true;
	
	try
	{
		auto StreamMeta = GetStreamMeta( Packet->mMeta );
		std::shared_ptr<Soy::TWriteProtocol> Mpeg2TsPacket( new Mpeg2Ts::TPesPacket( Packet, StreamMeta, mPacketCounter++ ) );

		UpdatePatPmt( *Packet );
		
		mOutput->Push( Mpeg2TsPacket );
	}
	catch (std::exception& e)
	{
		std::Debug << __func__ << " error; " << e.what() << std::endl;
	}
	
	return true;
}

void TMpeg2TsMuxer::UpdatePatPmt(const TMediaPacket& Packet)
{
	//	not written yet
	if ( !mPatPacket )
	{
		mPatPacket.reset( new Mpeg2Ts::TPatPacket( mProgramId ) );
		mOutput->Push( std::dynamic_pointer_cast<Soy::TWriteProtocol>( mPatPacket ) );
	}
	
	//	not written yet
	if ( !mPmtPacket )
	{
		mPmtPacket.reset( new Mpeg2Ts::TPmtPacket( mStreamMetas, mProgramId ) );
		mOutput->Push( std::dynamic_pointer_cast<Soy::TWriteProtocol>( mPmtPacket ) );
	}
}


Mpeg2Ts::TStreamMeta& TMpeg2TsMuxer::GetStreamMeta(const ::TStreamMeta& Stream)
{
	{
		auto ExistingMetaIt = mStreamMetas.find( Stream.mStreamIndex );
		if ( ExistingMetaIt != mStreamMetas.end() )
			return ExistingMetaIt->second;
	}
	
	//	create new
	auto& StreamMeta = mStreamMetas[Stream.mStreamIndex];
	StreamMeta.mProgramId = mProgramId;
	
	switch ( Stream.mCodec )
	{
		case SoyMediaFormat::H264:
		case SoyMediaFormat::H264Ts:
			StreamMeta.mStreamId = AP4_MPEG2_TS_DEFAULT_STREAM_ID_VIDEO;	//	+index
			StreamMeta.mStreamType = AP4_MPEG2_STREAM_TYPE_AVC;
			break;
			/*
		case SoyMediaFormat::HEV1:
		case SoyMediaFormat::HVC1:
			StreamMeta.mStreamId = AP4_MPEG2_TS_DEFAULT_STREAM_ID_VIDEO;	//	+index
			StreamMeta.mStreamType = AP4_MPEG2_STREAM_TYPE_HEVC;
			break;

		case SoyMediaFormat::Mp4a:
			StreamMeta.mStreamId = AP4_MPEG2_TS_DEFAULT_STREAM_ID_AUDIO;	//	+index
			StreamMeta.mStreamType = AP4_MPEG2_STREAM_TYPE_ISO_IEC_13818_7;
			break;
			
		case SoyMediaFormat::Ac3:
		case SoyMediaFormat::Ec3:
			StreamMeta.mStreamId = AP4_MPEG2_TS_STREAM_ID_PRIVATE_STREAM_1;	//	+index
			StreamMeta.mStreamType = AP4_MPEG2_STREAM_TYPE_ATSC_AC3;
			break;
	*/
		default:
		{
			std::stringstream Error;
			Error << "Mpeg2Ts doesn't support format " << Stream.mCodec;
			throw Soy::AssertException( Error.str() );
		}
	}
	
	return StreamMeta;
}
