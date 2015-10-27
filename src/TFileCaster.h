#pragma once

#include "TCaster.h"
#include <fstream>

class TMediaPacket;
class TMediaEncoder;
class TMediaPacketBuffer;

	
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
	std::ofstream		mFileStream;
};


