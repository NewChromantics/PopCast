#pragma once

#include <SoyMedia.h>
#include <SoyProtocol.h>
#include <SoyStream.h>


namespace Mpeg2Ts
{
	class TPacket;
	class TPesPacket;	//	PES encoder
	class TPatPacket;
	class TPmtPacket;
	class TStreamMeta;
	class TProgramMeta;
}


class Mpeg2Ts::TProgramMeta
{
public:
	TProgramMeta(uint16 ProgramId,uint16 PmtPid) :
		mProgramId	( ProgramId ),
		mPmtPid		( PmtPid )
	{
	}
	TProgramMeta() :
		mProgramId	( 0 ),
		mPmtPid		( 0 )
	{
	}
public:
	uint16		mProgramId;
	uint16		mPmtPid;
};


class Mpeg2Ts::TStreamMeta
{
public:
	TStreamMeta() :
		mStreamId	( 0 ),
		mStreamType	( 0 ),
		mProgramId	( 0 )
	{
	}
	TStreamMeta(uint16 StreamId,uint8 StreamType,uint16 ProgramId) :
		mStreamId	( StreamId ),
		mProgramId	( ProgramId ),
		mStreamType	( StreamType )
	{
		//	https://en.wikipedia.org/wiki/MPEG_transport_stream#PCR
		Soy::Assert( ProgramId != 0x1FFF, "Pid 0x1FFF is reserved as a null program id");
	}
	
public:
	uint16			mStreamId;
	uint8			mStreamType;	//	codec identifier
	Array<uint8>	mDescriptor;	//	each audio/video item has a descriptor
	
	//	gr: this is 13bit...
	uint16			mProgramId;	//	can have multiple programs per TS stream. eg. pid1=BBC1(video+audio+subtitles) and pid2=BBC2(video+audio+subitles)
};



class Mpeg2Ts::TPacket : public Soy::TWriteProtocol
{
public:
	TPacket(const TStreamMeta& Stream,size_t PacketCounter);
	
//	virtual void			Encode(TStreamBuffer& Buffer) override;
	
protected:
	void					WriteHeader(bool PayloadStart,size_t PayloadSize,bool WithPcr,uint64 PcrSize,TStreamBuffer& Buffer);

public:
	TStreamMeta				mStreamMeta;
	size_t					mPacketContinuityCounter;
};


class Mpeg2Ts::TPesPacket : public Mpeg2Ts::TPacket
{
public:
	TPesPacket(std::shared_ptr<TMediaPacket> Packet,const TStreamMeta& Stream,size_t PacketCounter,std::shared_ptr<TMediaPacket>& SpsPacket,std::shared_ptr<TMediaPacket>& PpsPacket);
	
	virtual void			Encode(TStreamBuffer& Buffer) override;
	
public:
	std::shared_ptr<TMediaPacket>&	mSpsPacket;
	std::shared_ptr<TMediaPacket>&	mPpsPacket;
	std::shared_ptr<TMediaPacket>	mPacket;
};



class Mpeg2Ts::TPatPacket : public Mpeg2Ts::TPacket
{
public:
	TPatPacket(ArrayBridge<TProgramMeta>&& Programs);

	virtual void			Encode(TStreamBuffer& Buffer) override;
	
	Array<TProgramMeta>		mPrograms;
};

class Mpeg2Ts::TPmtPacket : public Mpeg2Ts::TPacket
{
public:
	TPmtPacket(const std::map<size_t,Mpeg2Ts::TStreamMeta>& Streams,TProgramMeta ProgramMeta);
	
	virtual void	Encode(TStreamBuffer& Buffer) override;
	
	Array<uint8>	mPayload;
};


class TMpeg2TsMuxer : public TMediaMuxer
{
public:
	TMpeg2TsMuxer(std::shared_ptr<TStreamWriter>& Output,std::shared_ptr<TMediaPacketBuffer>& Input);
	
protected:
//	virtual void			SetupStreams(const ArrayBridge<TStreamMeta>&& Streams) override;
	virtual void			ProcessPacket(std::shared_ptr<TMediaPacket> Packet,TStreamWriter& Output) override;
	Mpeg2Ts::TStreamMeta&	GetStreamMeta(const ::TStreamMeta& Stream);
	void					UpdatePatPmt(const TMediaPacket& Packet);
	
public:
	Array<Mpeg2Ts::TProgramMeta>			mPrograms;
	std::map<size_t,Mpeg2Ts::TStreamMeta>	mStreamMetas;
	size_t									mPacketCounter;

	//	copies of the PAT/PMT if they've been written
	std::shared_ptr<Mpeg2Ts::TPatPacket>	mPatPacket;
	std::shared_ptr<Mpeg2Ts::TPmtPacket>	mPmtPacket;
	std::shared_ptr<TMediaPacket>			mSpsPacket;
	std::shared_ptr<TMediaPacket>			mPpsPacket;
	
};



