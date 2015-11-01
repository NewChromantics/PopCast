#pragma once

#include "TCaster.h"
#include <fstream>
#include <SoyStream.h>
#include "SoyMpeg2Ts.h"


class TMediaPacket;
class TMediaEncoder;
class TMediaPacketBuffer;


/*
class SoyMediaAtom
{
public:
	virtual void	Write(TStreamBuffer& Data);
	virtual void	WriteFields(TStreamBuffer& Data);
	
protected:
	virtual bool	IsFullAtom()	{	return false;	}
	virtual uint32	GetType()=0;
	virtual size_t	GetHeaderSize()	{	return AP4_ATOM_HEADER_SIZE;	}

private:
	virtual void	GetHeaderSize(uint32& Size32,uint64& Size64);
	
public:
};



class SoyMediaAtom_FTYP : public SoyMediaAtom
{
public:
	virtual void	Write(TStreamBuffer& Data);

protected:
	virtual bool	IsFullAtom()		{	return false;	}
	virtual uint32	GetType() override	{	return AP4_ATOM_TYPE_FTYP;	}
	virtual size_t	GetHeaderSize() override	{	return SoyMediaAtom::GetHeaderSize() + XX + compatbielsize;		}
	
public:
	uint32			mBrand;
	uint32			mBrandVersion;
	Array<uint32>	mCompatbileBrands;
};


class SoyMediaAtom_MOOV : public SoyMediaAtom
{
protected:
};


class SoyMediaAtom_MDAT : public SoyMediaAtom
{
protected:
	virtual bool	IsFullAtom()		{	return false;	}
	virtual uint32	GetType() override	{	return AP4_ATOM_TYPE_MDAT;	}
	virtual size_t	GetHeaderSize() override	{	return mdat_size;		}
};


class TMultiplexerTrack
{
public:
	void		RecalcChunkOffsets();
};
 */

class TMultiplexerMp4
{
public:
	TMultiplexerMp4(TStreamBuffer& Output);
	
	void			Push(TMediaPacket& Packet);
	
	void			WriteHeader();
	
public:
 /*
	SoyMediaAtom_FTYP			mFileAtom;
	SoyMediaAtom_MOOV			mMoovAtom;
	Array<SoyMediaAtom>			mSubAtoms;
	Array<TMultiplexerTrack>	mTracks;
	*/
	TStreamBuffer&				mOutput;
};



	
class TFileCaster : public TCaster, public SoyWorkerThread
{
public:
	TFileCaster(const TCasterParams& Params);
	~TFileCaster();
	
	virtual void		Write(const Opengl::TTexture& Image,SoyTime Timecode,Opengl::TContext& Context) override;
	virtual void		Write(const std::shared_ptr<SoyPixelsImpl> Image,SoyTime Timecode) override;
	
protected:
	virtual bool		Iteration() override;
	virtual bool		CanSleep() override;

protected:
	std::shared_ptr<TMediaEncoder>		mEncoder;
	std::shared_ptr<TMediaPacketBuffer>	mFrameBuffer;
	std::shared_ptr<TMpeg2TsMuxer>		mMuxer;
	std::shared_ptr<TStreamWriter>		mFileStream;
};

