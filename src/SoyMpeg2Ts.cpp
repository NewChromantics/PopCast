#include "SoyMpeg2Ts.h"


namespace Mpeg2Ts
{
	uint32	ComputeCRC(const unsigned char* data, unsigned int data_size);
	uint64	GetTimecode90hz(SoyTime Time);
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

uint64 Mpeg2Ts::GetTimecode90hz(SoyTime TimeMs)
{
	double Time = TimeMs.GetTime();
	Time /= 1000.0;
	Time *= 90.0;
	return Time;
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
	unsigned int space = 8-(mBitCount%8);
	while (bit_count)
	{
		auto Byte = mBitCount/8;
		while ( Byte >= mData.GetDataSize() )
		{
			mData.PushBack(0);
		}
		Soy::Assert( mBitCount/8 < mData.GetDataSize(), "bit out of bounds");
		auto& data = mData[mBitCount/8];
		unsigned int mask = bit_count==32 ? 0xFFFFFFFF : ((1<<bit_count)-1);
		if (bit_count <= space)
		{
			data |= ((bits&mask) << (space-bit_count));
			mBitCount += bit_count;
			return;
		}
		else
		{
			data |= ((bits&mask) >> (bit_count-space));
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

void Mpeg2Ts::TPacket::WriteHeader(bool PayloadStart,size_t PayloadSize,bool WithPcr,uint64 PcrSize,TStreamBuffer& Buffer)
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


Mpeg2Ts::TPesPacket::TPesPacket(std::shared_ptr<TMediaPacket> Packet,const TStreamMeta& Stream,size_t PacketCounter,std::shared_ptr<TMediaPacket>& SpsPacket,std::shared_ptr<TMediaPacket>& PpsPacket) :
	mPacket						( Packet ),
	TPacket						( Stream, PacketCounter ),
	mSpsPacket					( SpsPacket ),
	mPpsPacket					( PpsPacket )
{
	Soy::Assert( mPacket !=nullptr, "Packet missing");
}


void Mpeg2Ts::TPesPacket::Encode(TStreamBuffer& Buffer)
{
	std::Debug << "Encoding PES packet" << std::endl;
	//	ALLL the code below seems to be for converting to H264_ts, so assume it's already been done
	/*
	if (sample_description->GetType() == AP4_SampleDescription::TYPE_AVC) {
		// check the sample description
		AP4_AvcSampleDescription* avc_desc = AP4_DYNAMIC_CAST(AP4_AvcSampleDescription, sample_description);
		if (avc_desc == NULL) return AP4_ERROR_NOT_SUPPORTED;
		
		if ((int)sample.GetDescriptionIndex() != m_SampleDescriptionIndex) {
			m_SampleDescriptionIndex = (int)sample.GetDescriptionIndex();
			m_NaluLengthSize = avc_desc->GetNaluLengthSize();
			
			// make the SPS/PPS prefix
			m_Prefix.SetDataSize(0);
			for (unsigned int i=0; i<avc_desc->GetSequenceParameters().ItemCount(); i++) {
				AP4_DataBuffer& buffer = avc_desc->GetSequenceParameters()[i];
				unsigned int prefix_size = m_Prefix.GetDataSize();
				m_Prefix.SetDataSize(prefix_size+4+buffer.GetDataSize());
				unsigned char* p = m_Prefix.UseData()+prefix_size;
				*p++ = 0;
				*p++ = 0;
				*p++ = 0;
				*p++ = 1;
				AP4_CopyMemory(p, buffer.GetData(), buffer.GetDataSize());
			}
			for (unsigned int i=0; i<avc_desc->GetPictureParameters().ItemCount(); i++) {
				AP4_DataBuffer& buffer = avc_desc->GetPictureParameters()[i];
				unsigned int prefix_size = m_Prefix.GetDataSize();
				m_Prefix.SetDataSize(prefix_size+4+buffer.GetDataSize());
				unsigned char* p = m_Prefix.UseData()+prefix_size;
				*p++ = 0;
				*p++ = 0;
				*p++ = 0;
				*p++ = 1;
				AP4_CopyMemory(p, buffer.GetData(), buffer.GetDataSize());
			}
		}
	} else if (sample_description->GetType() == AP4_SampleDescription::TYPE_HEVC) {
		// check the sample description
		AP4_HevcSampleDescription* hevc_desc = AP4_DYNAMIC_CAST(AP4_HevcSampleDescription, sample_description);
		if (hevc_desc == NULL) return AP4_ERROR_NOT_SUPPORTED;
		
		if ((int)sample.GetDescriptionIndex() != m_SampleDescriptionIndex) {
			m_SampleDescriptionIndex = (int)sample.GetDescriptionIndex();
			m_NaluLengthSize = hevc_desc->GetNaluLengthSize();
			
			// make the VPS/SPS/PPS prefix
			m_Prefix.SetDataSize(0);
			for (unsigned int i=0; i<hevc_desc->GetSequences().ItemCount(); i++) {
				const AP4_HvccAtom::Sequence& seq = hevc_desc->GetSequences()[i];
				if (seq.m_NaluType == AP4_HEVC_NALU_TYPE_VPS_NUT) {
					for (unsigned int j=0; j<seq.m_Nalus.ItemCount(); j++) {
						const AP4_DataBuffer& buffer = seq.m_Nalus[j];
						unsigned int prefix_size = m_Prefix.GetDataSize();
						m_Prefix.SetDataSize(prefix_size+4+buffer.GetDataSize());
						unsigned char* p = m_Prefix.UseData()+prefix_size;
						*p++ = 0;
						*p++ = 0;
						*p++ = 0;
						*p++ = 1;
						AP4_CopyMemory(p, buffer.GetData(), buffer.GetDataSize());
					}
				}
			}
			
			for (unsigned int i=0; i<hevc_desc->GetSequences().ItemCount(); i++) {
				const AP4_HvccAtom::Sequence& seq = hevc_desc->GetSequences()[i];
				if (seq.m_NaluType == AP4_HEVC_NALU_TYPE_SPS_NUT) {
					for (unsigned int j=0; j<seq.m_Nalus.ItemCount(); j++) {
						const AP4_DataBuffer& buffer = seq.m_Nalus[j];
						unsigned int prefix_size = m_Prefix.GetDataSize();
						m_Prefix.SetDataSize(prefix_size+4+buffer.GetDataSize());
						unsigned char* p = m_Prefix.UseData()+prefix_size;
						*p++ = 0;
						*p++ = 0;
						*p++ = 0;
						*p++ = 1;
						AP4_CopyMemory(p, buffer.GetData(), buffer.GetDataSize());
					}
				}
			}
			
			for (unsigned int i=0; i<hevc_desc->GetSequences().ItemCount(); i++) {
				const AP4_HvccAtom::Sequence& seq = hevc_desc->GetSequences()[i];
				if (seq.m_NaluType == AP4_HEVC_NALU_TYPE_PPS_NUT) {
					for (unsigned int j=0; j<seq.m_Nalus.ItemCount(); j++) {
						const AP4_DataBuffer& buffer = seq.m_Nalus[j];
						unsigned int prefix_size = m_Prefix.GetDataSize();
						m_Prefix.SetDataSize(prefix_size+4+buffer.GetDataSize());
						unsigned char* p = m_Prefix.UseData()+prefix_size;
						*p++ = 0;
						*p++ = 0;
						*p++ = 0;
						*p++ = 1;
						AP4_CopyMemory(p, buffer.GetData(), buffer.GetDataSize());
					}
				}
			}
			
		}
	} else {
		return AP4_ERROR_NOT_SUPPORTED;
	}
	
	// decide if we need to emit the prefix
	bool emit_prefix = false;
	if (sample.IsSync() || m_SamplesWritten == 0) {
		emit_prefix = true;
	}
	
	// write the NAL units
	const unsigned char* data      = sample_data.GetData();
	unsigned int         data_size = sample_data.GetDataSize();
	
	// allocate a buffer for the PES packet
	AP4_DataBuffer pes_data;
	
	// output all NALUs
	for (unsigned int nalu_count = 0; data_size; nalu_count++) {
		// sanity check
		if (data_size < m_NaluLengthSize) break;
		
		// get the next NAL unit
		AP4_UI32 nalu_size;
		if (m_NaluLengthSize == 1) {
			nalu_size = *data++;
			data_size--;
		} else if (m_NaluLengthSize == 2) {
			nalu_size = AP4_BytesToInt16BE(data);
			data      += 2;
			data_size -= 2;
		} else if (m_NaluLengthSize == 4) {
			nalu_size = AP4_BytesToInt32BE(data);
			data      += 4;
			data_size -= 4;
		} else {
			break;
		}
		if (nalu_size > data_size) break;
		
		// check if we need to add a delimiter before the NALU
		if (nalu_count == 0 && sample_description->GetType() == AP4_SampleDescription::TYPE_AVC) {
			if (nalu_size != 2 || (data[0] & 0x1F) != AP4_AVC_NAL_UNIT_TYPE_ACCESS_UNIT_DELIMITER) {
				// the first NAL unit is not an Access Unit Delimiter, we need to add one
				unsigned char delimiter[6];
				delimiter[0] = 0;
				delimiter[1] = 0;
				delimiter[2] = 0;
				delimiter[3] = 1;
				delimiter[4] = 9;    // NAL type = Access Unit Delimiter;
				delimiter[5] = 0xF0; // Slice types = ANY
				pes_data.AppendData(delimiter, 6);
				
				if (emit_prefix) {
					pes_data.AppendData(m_Prefix.GetData(), m_Prefix.GetDataSize());
					emit_prefix = false;
				}
			}
		} else {
			if (emit_prefix) {
				pes_data.AppendData(m_Prefix.GetData(), m_Prefix.GetDataSize());
				emit_prefix = false;
			}
		}
		
		// add a start code before the NAL unit
		unsigned char start_code[3];
		start_code[0] = 0;
		start_code[1] = 0;
		start_code[2] = 1;
		pes_data.AppendData(start_code, 3);
		
		// add the NALU
		pes_data.AppendData(data, nalu_size);
		
		// for AVC streams that do start with a NAL unit delimiter, we need to add the prefix now
		if (emit_prefix) {
			pes_data.AppendData(m_Prefix.GetData(), m_Prefix.GetDataSize());
			emit_prefix = false;
		}
		
		// move to the next NAL unit
		data      += nalu_size;
		data_size -= nalu_size;
	}
	
	// compute the timestamp
	AP4_UI64 dts = AP4_ConvertTime(sample.GetDts(), m_TimeScale, 90000);
	AP4_UI64 pts = AP4_ConvertTime(sample.GetCts(), m_TimeScale, 90000);
	
	// update counters
	++m_SamplesWritten;
	
	// write the packet
	return WritePES(pes_data.GetData(), pes_data.GetDataSize(), dts, true, pts, with_pcr, output);
}
	 */
	/*
	uint64
	AP4_ConvertTime(uint64 time_value,
					uint32 from_time_scale,
					uint32 to_time_scale)
	{
		if (from_time_scale == 0) return 0;
		double ratio = (double)to_time_scale/(double)from_time_scale;
		return ((AP4_UI64)(0.5+(double)time_value*ratio));
	}
	
	uint64 PresentationTime = Packet.mTimecode;
	uint64 DisplayTimestamp =
	AP4_UI64 dts = AP4_ConvertTime(sample.GetDts(), m_TimeScale, 90000);
	AP4_UI64 pts = AP4_ConvertTime(sample.GetCts(), m_TimeScale, 90000);
	
	*/
	
	auto& Packet = *mPacket;
	Array<uint8> PacketData;
	if ( mSpsPacket )
	{
		PacketData.PushBackArray( mSpsPacket->mData );
		mSpsPacket.reset();
	}
	if ( mPpsPacket )
	{
		PacketData.PushBackArray( mPpsPacket->mData );
		mPpsPacket.reset();
	}
	PacketData.PushBackArray( Packet.mData );

	
	uint64 pts = GetTimecode90hz( Packet.mTimecode );
	if ( pts == 0 )
		pts = mPacketContinuityCounter * 90;
	uint64 dts = GetTimecode90hz( Packet.mDecodeTimecode );
	// adjust the base timestamp so we don't start at 0
	// dts += 10000;
	// pts += 10000;

	
	Array<uint8> AdditionalHeader;
	{
		bool with_dts = (dts != 0);
		TArrayBitWriter AdditionalHeaderWriter( GetArrayBridge(AdditionalHeader) );
		if ( pts != 0 )
		{
			//AdditionalHeaderWriter.Write(PtsDtsFlag, 2); // PTS_DTS_flags
			AdditionalHeaderWriter.Write(with_dts?3:2, 4);         // '0010' or '0011'
			AdditionalHeaderWriter.Write((uint32)(pts>>30), 3);  // PTS[32..30]
			AdditionalHeaderWriter.Write(1, 1);                    // marker_bit
			AdditionalHeaderWriter.Write((uint32)(pts>>15), 15); // PTS[29..15]
			AdditionalHeaderWriter.Write(1, 1);                    // marker_bit
			AdditionalHeaderWriter.Write((uint32)pts, 15);       // PTS[14..0]
			AdditionalHeaderWriter.Write(1, 1);                    // market_bit
		}
		
		if ( dts != 0 )
		{
			AdditionalHeaderWriter.Write(1, 4);                    // '0001'
			AdditionalHeaderWriter.Write((uint32)(dts>>30), 3);  // DTS[32..30]
			AdditionalHeaderWriter.Write(1, 1);                    // marker_bit
			AdditionalHeaderWriter.Write((uint32)(dts>>15), 15); // DTS[29..15]
			AdditionalHeaderWriter.Write(1, 1);                    // marker_bit
			AdditionalHeaderWriter.Write((uint32)dts, 15);       // DTS[14..0]
			AdditionalHeaderWriter.Write(1, 1);                    // market_bit
		}
	}
	
	//unsigned int pes_header_size = 14+(with_dts?5:0);
	unsigned int pes_header_size = 9;
	
	Array<uint8> Header;
	TArrayBitWriter pes_header( GetArrayBridge(Header) );
	
	
	pes_header.Write(0x000001, 24);    // packet_start_code_prefix
	pes_header.Write( mStreamMeta.mStreamId, 8);   // stream_id

	//	https://en.wikipedia.org/wiki/Packetized_elementary_stream#cite_note-7
	//	Specifies the number of bytes remaining in the packet after this field.
	//	Can be zero. If the PES packet length is set to zero, the PES packet can be of any length.
	//	A value of zero for the PES packet length can be used only when the PES packet payload is a
	//	video elementary stream
	//uint16 PacketLength = (mStreamMeta.mStreamId == AP4_MPEG2_TS_DEFAULT_STREAM_ID_VIDEO) ? 0 : (data_size+pes_header_size-6);
	//	video can be 0 because the length can be > 16bit
	//	gr: -6 for everything before length...
	size_t PacketLength_t = ( PacketData.GetDataSize() + pes_header_size + AdditionalHeader.GetDataSize() - 6 );
	uint16 PacketLength = PacketLength_t > 65536 ? 0 : PacketLength_t;
	pes_header.Write(PacketLength, 16); // PES_packet_length
	
	pes_header.Write(2, 2);            // '01'
	pes_header.Write(0, 2);            // PES_scrambling_control
	pes_header.Write(0, 1);            // PES_priority
	pes_header.Write(1, 1);            // data_alignment_indicator
	pes_header.Write(0, 1);            // copyright
	pes_header.Write(0, 1);            // original_or_copy
	
	uint32 PtsDtsFlag = 0;
	if ( pts != 0 )	PtsDtsFlag |= 0x2;
	if ( dts != 0 )	PtsDtsFlag |= 0x1;
	//	pes_header.Write(with_dts?3:2, 2); // PTS_DTS_flags
	pes_header.Write(PtsDtsFlag, 2); // PTS_DTS_flags
	pes_header.Write(0, 1);            // ESCR_flag
	pes_header.Write(0, 1);            // ES_rate_flag
	pes_header.Write(0, 1);            // DSM_trick_mode_flag
	pes_header.Write(0, 1);            // additional_copy_info_flag
	pes_header.Write(0, 1);            // PES_CRC_flag
	pes_header.Write(0, 1);            // PES_extension_flag
	pes_header.Write( size_cast<uint32>(AdditionalHeader.GetDataSize()), 8);// PES_header_data_length
	
	//	write data, splitting as we go
	size_t DataWritten = 0;
	Array<uint8> Data;
	Data.PushBackArray( Header );
	Data.PushBackArray( AdditionalHeader );
	Data.PushBackArray( PacketData );
	
	while ( DataWritten < Data.GetDataSize() )
	{
		//	chunk
		size_t PayloadSize = Data.GetDataSize() - DataWritten;
		if ( PayloadSize > AP4_MPEG2TS_PACKET_PAYLOAD_SIZE )
		{
			PayloadSize = AP4_MPEG2TS_PACKET_PAYLOAD_SIZE;
		}
		auto PayloadData = GetArrayBridge(Data).GetSubArray<uint8>( DataWritten, PayloadSize );
		
		//	first packet
		if ( DataWritten == 0 )
		{
			//	wtf is this number
			bool WithPcr = false;
			auto PcrSize = ( dts!=0 ? dts : pts ) * 300;
			WriteHeader( true, PayloadData.GetDataSize(), WithPcr, PcrSize, Buffer );
		}
		else
		{
			auto PcrSize = 0;
			WriteHeader( false, PayloadData.GetDataSize(), false, PcrSize, Buffer );
		}
		Buffer.Push( GetArrayBridge(PayloadData) );
		DataWritten += PayloadData.GetDataSize();
	}

}


Mpeg2Ts::TPatPacket::TPatPacket(ArrayBridge<TProgramMeta>&& Programs) :
	TPacket			( TStreamMeta(0,0,0), 0 ),
	mPrograms		( Programs )
{
}

void Mpeg2Ts::TPatPacket::Encode(TStreamBuffer& Buffer)
{
	std::Debug << "Encoding PAT packet" << std::endl;

	bool PayloadStart = true;
	size_t PayloadSize = AP4_MPEG2TS_PACKET_PAYLOAD_SIZE;
	bool WithPcr = false;
	WriteHeader( PayloadStart, PayloadSize, WithPcr, 0, Buffer );
	
	Array<uint8> Payload;
	
	TArrayBitWriter writer( GetArrayBridge(Payload) );
	uint16 SectionLength = 9 + (4*mPrograms.GetSize());	//	header + section data
	uint16 TransportStreamId = 1;
	
	writer.Write(0, 8);  // pointer
	writer.Write(0, 8);  // table_id
	writer.Write(1, 1);  // section_syntax_indicator
	writer.Write(0, 1);  // '0'
	writer.Write(3, 2);  // reserved
	writer.Write(SectionLength, 12);// section_length
	writer.Write(TransportStreamId, 16); // transport_stream_id
	writer.Write(3, 2);  // reserved
	writer.Write(0, 5);  // version_number
	writer.Write(1, 1);  // current_next_indicator
	

	writer.Write( 0, 8);  // section_number
	writer.Write( size_cast<uint32>(mPrograms.GetSize()-1), 8);  // last_section_number
	
	//	section data
	for ( int i=0;	i<mPrograms.GetSize();	i++ )
	{
		uint16 Pid = mPrograms[i].mProgramId;
		uint16 PmtPid = mPrograms[i].mPmtPid;
		writer.Write( Pid, 16); // program number
		writer.Write(7, 3);  // reserved
		writer.Write( PmtPid, 13); // program_map_PID
	}
	
	
	uint8 SectionLengthHi = static_cast<uint8>(SectionLength>>8);
	uint8 SectionLengthLo = static_cast<uint8>(SectionLength&0xff);
	uint16 Pid = mPrograms[0].mProgramId;
	uint16 PmtPid = mPrograms[0].mPmtPid;
	uint8 HardCodedPayload[] =
	{
		0x00,	//	pointer
		0x00,	//	table id
		static_cast<uint8>(0x10|0x00|0x30|SectionLengthHi), SectionLengthLo,	//	section, 0, reserved, section length
		static_cast<uint8>(TransportStreamId>>8),	static_cast<uint8>(TransportStreamId&0xff),	//	transport stream id
		0xC1,	//	reserved, version, current next indicator
		
		0x00,	//	section number
		0x00,	//	last section number
		static_cast<uint8>(Pid>>8),	static_cast<uint8>(Pid&0xFF),	//	program number

		static_cast<uint8>(0xE0|(PmtPid>>8)), static_cast<uint8>(PmtPid&0xFF),		//	0x7reserved, program map pid
//		0x36, 0x90, 0xE2, 0x3D	//	checksum
	};

	static bool UseHardcoded = false;
	if ( UseHardcoded )
	{
		Payload.Clear();
		Payload.PushBackArray( HardCodedPayload );
	}
	Buffer.Push( GetArrayBridge(Payload) );
	
	{
		//auto CrcLength = 17-1-4;
		auto CrcLength = Payload.GetSize() - 1;
		Array<uint8> CrcArray;
		TArrayBitWriter CrcWriter( GetArrayBridge(CrcArray) );
		auto Crc = ComputeCRC( &Payload[1], CrcLength );
		CrcWriter.Write( Crc, 32 );
		Buffer.Push( GetArrayBridge(CrcArray) );
	}

	
	Array<uint8> Padding;
	Padding.SetSize( AP4_MPEG2TS_PACKET_PAYLOAD_SIZE-Payload.GetDataSize() );
	Padding.SetAll( 0xff );
	Buffer.Push( GetArrayBridge(Padding) );

}

Mpeg2Ts::TPmtPacket::TPmtPacket(const std::map<size_t,Mpeg2Ts::TStreamMeta>& Streams,TProgramMeta ProgramMeta) :
	TPacket	( TStreamMeta(0,0,ProgramMeta.mPmtPid), 0 )
{
	Array<uint8> Payload;
	TArrayBitWriter writer( GetArrayBridge(Payload) );
	
	//	http://www.etherguidesystems.com/help/sdos/mpeg/semantics/mpeg-2/section_length.aspx
	//	The section_length field is a 12-bit field that gives the length of the table section beyond this
	//	field. Since it is carried starting at bit index 12 in the section (the second and third bytes),
	//	the actual size of the table section is section_length + 3.
	unsigned int section_length = 13;	//	gr: seems 1 too many...
	uint16 pcr_pid = 0;
	//	http://www.etherguidesystems.com/Help/SDOs/MPEG/semantics/mpeg-2/PCR_PID.aspx
	//	The PCR_PID is the packet id where the program clock reference for

	for ( auto it=Streams.begin();	it!=Streams.end();	it++ )
	{
		auto& Stream = it->second;
		if ( Stream.mProgramId != ProgramMeta.mProgramId )
			continue;
		//Soy::Assert( Stream.mProgramId == pcr_pid, "Stream has mis-matched program id... skip, or error?" );
		
		section_length += 5+Stream.mDescriptor.GetDataSize();
		
		//	assign a PCR
		if ( pcr_pid == 0 )
			pcr_pid = Stream.mProgramId;
	}

	Array<uint8> ProgramData;
	section_length += ProgramData.GetSize();
	
	//	http://www.etherguidesystems.com/Help/SDOs/MPEG/Syntax/TableSections/Pmts.aspx
	writer.Write(0, 8);        // pointer
	writer.Write(2, 8);        // table_id
	writer.Write(1, 1);        // section_syntax_indicator
	writer.Write(0, 1);        // '0'
	writer.Write(3, 2);        // reserved
	writer.Write(section_length, 12); // section_length
	writer.Write(ProgramMeta.mProgramId, 16);       // program_number
	writer.Write(3, 2);        // reserved
	writer.Write(0, 5);        // version_number
	writer.Write(1, 1);        // current_next_indicator
	writer.Write(0, 8);        // section_number
	writer.Write(0, 8);        // last_section_number
	writer.Write(7, 3);        // reserved
	writer.Write(pcr_pid, 13); // PCD_PID
	writer.Write(0xF, 4);      // reserved
	writer.Write( size_cast<uint32>(ProgramData.GetSize()), 12);       // program_info_length
	for ( int i=0;	i<ProgramData.GetDataSize(); i++ )
	{
		writer.Write( ProgramData[i], 8 );
	}
	
	for ( auto it=Streams.begin();	it!=Streams.end();	it++ )
	{
		auto& Stream = it->second;
		if ( Stream.mProgramId != ProgramMeta.mProgramId )
			continue;

		writer.Write( Stream.mStreamType, 8);                // stream_type
		writer.Write(0x7, 3);                                  // reserved
		writer.Write( Stream.mProgramId, 13);                   // elementary_PID
		writer.Write(0xF, 4);                                  // reserved
		writer.Write( size_cast<uint32>(Stream.mDescriptor.GetDataSize()), 12); // ES_info_length

		for ( int i=0;	i<Stream.mDescriptor.GetDataSize(); i++ )
		{
			writer.Write( Stream.mDescriptor[i], 8 );
		}
	}

	/*
	auto Crc = ComputeCRC( &Payload[1], section_length-1);
	writer.Write(Crc, 32); // CRC

	Payload.SetSize( section_length+4 );
*/

	//uint8 HardCodedPayload[] = { 0x00, 0x00, 0xB0, 0x0D, 0x00, 0x01, 0xC1, 0x00, 0x00, 0x00, 0x01, 0xEF, 0xFF, 0x36, 0x90, 0xE2, 0x3D	};
	uint8 HardCodedPayload[] =
	{
		0x00,	//	pointer
		0x02,	//	table id
		0xB0,	0x3C,	//	section_syntax_indicator 0 reserved section_length
		0x00,	0x01,	//	program_number
		0xC1,	//	reserved version_number current_next_indicator
		0x00,	//	section number
		0x00,	//	last_section_number
		static_cast<uint8>(0xE0|static_cast<uint8>(pcr_pid>>8)), static_cast<uint8>(pcr_pid&0xff),	//	reserved PCD_PID
		0xF|0, 0x11,	//	reserved program_info_length
		//	program info (0x11=17)
		0x25, 0x0F, 0xFF, 0xFF, 0x49, 0x44, 0x33, 0x20, 0xFF, 0x49, 0x44, 0x33, 0x20, 0x00, 0x1F, 0x00, 0x01,
		
		0x15,	//	stream type	ffprobe: Data: timed_id3 (ID3  / 0x20334449)
		0xE0|0x01, 0x02,	//	reserved | programid
		0xF0|0x00, 0x0F,	//	reserved | ES_info_length
		0x26, 0x0D, 0xFF, 0xFF, 0x49, 0x44, 0x33, 0x20, 0xFF, 0x49, 0x44, 0x33, 0x20, 0x00, 0x0F,
		
		AP4_MPEG2_STREAM_TYPE_AVC,	//	stream type
		0xE0|0x01, 0x00,	//	reserved | programid
		0xF0|0x00, 0x00,	//	reserved | ES_info_length
		
		AP4_MPEG2_STREAM_TYPE_ISO_IEC_13818_7,	//	stream type
		0xE0|0x01, 0x01,	//	reserved | programid
		0xF0|0x00, 0x00,	//	reserved | ES_info_length
		
		//	checksum
	//	0xD2, 0x7F, 0x16, 0x89
	};
	static bool UseHardcoded = false;
	if ( UseHardcoded )
	{
		Payload.Clear();
		Payload.PushBackArray( HardCodedPayload );
	}

	mPayload.PushBackArray( Payload );
	{
		//auto CrcLength = section_length-1;
		auto CrcLength = mPayload.GetSize() - 1;
		Array<uint8> CrcArray;
		TArrayBitWriter CrcWriter( GetArrayBridge(CrcArray) );
		auto Crc = ComputeCRC( &Payload[1], CrcLength );
		CrcWriter.Write( Crc, 32 );
		mPayload.PushBackArray( GetArrayBridge(CrcArray) );
	}
	
	Array<uint8> Padding;
	Padding.SetSize( AP4_MPEG2TS_PACKET_PAYLOAD_SIZE-mPayload.GetDataSize() );
	Padding.SetAll( 0xff );
	mPayload.PushBackArray( GetArrayBridge(Padding) );

}

void Mpeg2Ts::TPmtPacket::Encode(TStreamBuffer& Buffer)
{
	std::Debug << "Encoding PMT packet" << std::endl;

	WriteHeader( true, AP4_MPEG2TS_PACKET_PAYLOAD_SIZE, false, 0, Buffer );
	Buffer.Push( GetArrayBridge( mPayload ) );
}




TMpeg2TsMuxer::TMpeg2TsMuxer(std::shared_ptr<TStreamWriter>& Output,std::shared_ptr<TMediaPacketBuffer>& Input) :
	TMediaMuxer		( Output, Input, "TMpeg2TsMuxer" ),
	mPacketCounter	( 0 )
{
	mPrograms.PushBack( Mpeg2Ts::TProgramMeta( AP4_MPEG2_TS_DEFAULT_PID_VIDEO, AP4_MPEG2_TS_DEFAULT_PID_PMT ) );
}


void TMpeg2TsMuxer::ProcessPacket(std::shared_ptr<TMediaPacket> pPacket,TStreamWriter& Output)
{
	auto& Packet = *pPacket;
	static bool HoldSps = true;
	if ( HoldSps )
	{
		if ( Packet.mMeta.mCodec == SoyMediaFormat::H264_SPS_ES )
		{
			mSpsPacket = pPacket;
			return;
		}
		
		if ( Packet.mMeta.mCodec == SoyMediaFormat::H264_PPS_ES )
		{
			mPpsPacket = pPacket;
			return;
		}
	}
	
	auto StreamMeta = GetStreamMeta( Packet.mMeta );

	UpdatePatPmt( Packet );

	static bool WriteSps = true;

	if ( Packet.mMeta.mCodec == SoyMediaFormat::H264_SPS_ES || Packet.mMeta.mCodec == SoyMediaFormat::H264_PPS_ES )
	{
		if ( WriteSps )
		{
			std::shared_ptr<Soy::TWriteProtocol> Mpeg2TsPacket( new Mpeg2Ts::TPesPacket( pPacket, StreamMeta, mPacketCounter++, mSpsPacket, mPpsPacket ) );
			mOutput->Push( Mpeg2TsPacket );
		}
	}
	else
	{
		
		static bool WriteFrames = true;
		if ( WriteFrames )
		{
			std::shared_ptr<Soy::TWriteProtocol> Mpeg2TsPacket( new Mpeg2Ts::TPesPacket( pPacket, StreamMeta, mPacketCounter++, mSpsPacket, mPpsPacket ) );
			mOutput->Push( Mpeg2TsPacket );
		}
	}
}


void TMpeg2TsMuxer::UpdatePatPmt(const TMediaPacket& Packet)
{
	//	not written yet
	if ( !mPatPacket )
	{
		mPatPacket.reset( new Mpeg2Ts::TPatPacket( GetArrayBridge(mPrograms) ) );
		mOutput->Push( std::dynamic_pointer_cast<Soy::TWriteProtocol>( mPatPacket ) );
	}
	
	//	not written yet
	if ( !mPmtPacket )
	{
		for ( int i=0;	i<mPrograms.GetSize();	i++ )
		{
			mPmtPacket.reset( new Mpeg2Ts::TPmtPacket( mStreamMetas, mPrograms[i] ) );
			mOutput->Push( std::dynamic_pointer_cast<Soy::TWriteProtocol>( mPmtPacket ) );
		}
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
	StreamMeta.mProgramId = mPrograms[0].mProgramId;
	
	switch ( Stream.mCodec )
	{
		//case SoyMediaFormat::H264:	//	expecting data as NALU
		case SoyMediaFormat::H264_ES:
		case SoyMediaFormat::H264_SPS_ES:
		case SoyMediaFormat::H264_PPS_ES:
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
