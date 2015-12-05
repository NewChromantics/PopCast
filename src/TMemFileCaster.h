#pragma once

#include "TCaster.h"
#include <SoyMemFile.h>



class TMemFileHeader
{
public:
	static const char*	Magic; //	64 bit
	static const uint64	Version; //	64 bit
	
public:
	TMemFileHeader(const SoyPixelsMeta& Meta,SoyTime Time=SoyTime());			//	generate header which will tell us how much data we need
	
	void		Verify();		//	throws if magic or version is bad
	void		SetError(const std::string& Error);
	
	uint64		mMagic;
	uint64		mVersion;		//	version. 64 so we can use hash's later
	uint32		mDataSize;		//	NOT including header
	uint32		mPixelFormat;	//	SoyPixelsFormat::Type
	uint16		mWidth;
	uint16		mHeight;
	uint64		mTimecode;		//	ms
	char		mError[256];	//	error/debug string
};

	
	
class TMemFileCaster : public TCaster
{
public:
	TMemFileCaster(const TCasterParams& Params);
	
	virtual void		Write(const Opengl::TTexture& Image,const TCastFrameMeta& FrameMeta,Opengl::TContext& Context) override;
	virtual void		Write(const std::shared_ptr<SoyPixelsImpl> Image,const TCastFrameMeta& FrameMeta) override;
	
protected:
	void				AllocateFile(const std::string& Filename,const SoyPixelsMeta& Meta);

public:
	std::string			mFilename;			//	filename stored in case we do a deffered allocation
	SoyPixelsMeta		mAllocatedMeta;		//	meta that file was allocated with
	std::shared_ptr<SoyMemFile>	mMemFile;
};


